#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Preferences.h>
#include "LittleFS.h"
#include "driver/i2s.h"

// ========== SERIAL ==========
#define SERIAL_BAUD 115200

// ========== I2S (MAX98357A) ==========
#define I2S_BCLK 18
#define I2S_LRC  19
#define I2S_DOUT 23

// ========== I2C (MPU6050) ==========
#define SDA_PIN 21
#define SCL_PIN 22

// ========== WAV format ==========
#define WAV_RATE 8000

// цифровая громкость (1..3)
#define DIGITAL_GAIN 2

// ====== ЖЁСТКИЕ ПОРОГИ (как ты просила) ======
const float TH   = 30.0f;   // основной порог
const float TH2  = 55.0f;   // “сильный” порог для фраз 3 и 6
const float DEAD = 15.0f;   // нейтральная зона
const int   STABLE_FRAMES = 12;    // стабилизация
const unsigned long NEU_HOLD_MS = 250;  // сколько держать NEU чтобы снова “вооружиться”

// MPU адреса
const uint8_t MPU_L = 0x68; // LEFT  (AD0->GND)
const uint8_t MPU_R = 0x69; // RIGHT (AD0->3.3V)

enum Gest : uint8_t { NEU, UP, DOWN, LEFT, RIGHT };

struct MpuData {
  int16_t ax, ay, az;
  float pitch, roll;
};

Preferences prefs;

// ====== MPU helpers ======
void mpuWrite(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}

bool mpuRead14(uint8_t addr, uint8_t startReg, uint8_t* buf) {
  Wire.beginTransmission(addr);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false;
  int n = Wire.requestFrom((int)addr, 14, (int)true);
  if (n != 14) return false;
  for (int i = 0; i < 14; i++) buf[i] = Wire.read();
  return true;
}

bool readMpu(uint8_t addr, MpuData &d) {
  uint8_t b[14];
  if (!mpuRead14(addr, 0x3B, b)) return false;

  d.ax = (int16_t)((b[0] << 8) | b[1]);
  d.ay = (int16_t)((b[2] << 8) | b[3]);
  d.az = (int16_t)((b[4] << 8) | b[5]);

  float ax = d.ax, ay = d.ay, az = d.az;
  d.roll  = atan2f(ay, az) * 57.2958f;
  d.pitch = atan2f(-ax, sqrtf(ay*ay + az*az)) * 57.2958f;
  return true;
}

// ====== math helpers ======
float angleDiff(float a, float b) {
  float d = a - b;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

float ema(float prev, float x, float a = 0.2f) {
  return prev + a * (x - prev);
}

// ====== Calibration stored ======
float p0L=0, r0L=0, p0R=0, r0R=0;
bool calibrated=false;

void saveCal() {
  prefs.begin("cal", false);
  prefs.putBool("ok", true);
  prefs.putFloat("p0L", p0L);
  prefs.putFloat("r0L", r0L);
  prefs.putFloat("p0R", p0R);
  prefs.putFloat("r0R", r0R);
  prefs.end();
}

bool loadCal() {
  prefs.begin("cal", true);
  bool ok = prefs.getBool("ok", false);
  if (ok) {
    p0L = prefs.getFloat("p0L", 0);
    r0L = prefs.getFloat("r0L", 0);
    p0R = prefs.getFloat("p0R", 0);
    r0R = prefs.getFloat("r0R", 0);
  }
  prefs.end();
  return ok;
}

void applyHandFixes(MpuData &L, MpuData &R) {
  // Эти правки оставляем как раньше (под “MPU лицом к перчатке”):
  L.pitch = -L.pitch;  // левую по pitch зеркалим
  R.roll  = -R.roll;   // правую по roll зеркалим
}

void calibrateZero() {
  Serial.println("CAL: hold BOTH hands still...");
  const int N = 90;
  float spL=0, srL=0, spR=0, srR=0;
  int ok=0;

  for (int i=0;i<N;i++){
    MpuData L,R;
    if (readMpu(MPU_L, L) && readMpu(MPU_R, R)) {
      applyHandFixes(L, R);
      spL += L.pitch; srL += L.roll;
      spR += R.pitch; srR += R.roll;
      ok++;
    }
    delay(20);
  }

  if (ok > 20) {
    p0L = spL/ok; r0L = srL/ok;
    p0R = spR/ok; r0R = srR/ok;
    calibrated = true;
    saveCal();
    Serial.println("CAL: OK (saved)");
  } else {
    calibrated = false;
    Serial.println("CAL: FAILED (check 0x68/0x69 wiring)");
  }
}

// ====== ЖЕСТЫ: одна ось на руку (без рандома) ======
// Правая рука: только dpR (pitch delta) -> UP/DOWN + “сильный” порог TH2
Gest gestRightFromPitch(float dpR) {
  if (fabs(dpR) < DEAD) return NEU;
  if (dpR > TH)  return UP;
  if (dpR < -TH) return DOWN;
  return NEU;
}

// Левая рука: только drL (roll delta) -> LEFT/RIGHT + “сильный” порог TH2
Gest gestLeftFromRoll(float drL) {
  if (fabs(drL) < DEAD) return NEU;
  if (drL > TH)  return RIGHT;
  if (drL < -TH) return LEFT;
  return NEU;
}

// ====== 6 фраз, строго: 1-3 только правая, 4-6 только левая ======
// 1: правая UP (обычно)
// 2: правая DOWN (обычно)
// 3: правая UP “сильно” (TH2)   [или можно сделать DOWN сильно — см. ниже]
// 4: левая LEFT (обычно)
// 5: левая RIGHT (обычно)
// 6: левая RIGHT “сильно” (TH2) [или LEFT сильно — см. ниже]
int phraseForOneAxis(float dpR, float drL, Gest gR, Gest gL) {
  // правая активна только если левая NEU
  if (gL == NEU) {
    if (gR == UP) {
      if (dpR > TH2) return 3;   // сильный UP
      return 1;
    }
    if (gR == DOWN) {
      return 2;
      // если хочешь: сильный DOWN -> фраза 3, замени на:
      // if (dpR < -TH2) return 3; else return 2;
    }
  }

  // левая активна только если правая NEU
  if (gR == NEU) {
    if (gL == LEFT)  return 4;
    if (gL == RIGHT) {
      if (drL > TH2) return 6;   // сильный RIGHT
      return 5;
      // если хочешь: сильный LEFT -> фраза 6, замени на:
      // if (drL < -TH2) return 6; else return 4;
    }
  }

  return 0;
}

const char* wavForPhrase(int id){
  switch(id){
    case 1: return "/p1.wav";
    case 2: return "/p2.wav";
    case 3: return "/p3.wav";
    case 4: return "/p4.wav";
    case 5: return "/p5.wav";
    case 6: return "/p6.wav";
  }
  return "";
}

// ====== I2S / WAV ======
void i2sInit() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = WAV_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; // L=R
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.tx_desc_auto_clear = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num  = I2S_LRC;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

uint32_t readLE32(File &f) {
  uint8_t b[4]; f.read(b,4);
  return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}

bool skipToDataChunk(File &f) {
  f.seek(12);
  while (true) {
    if (f.available() < 8) return false;
    char id[5] = {0};
    f.read((uint8_t*)id, 4);
    uint32_t size = readLE32(f);
    if (!strcmp(id, "data")) return true;
    f.seek(f.position() + size);
  }
}

volatile bool playing=false;

void playWav(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) { Serial.print("NO FILE "); Serial.println(path); return; }

  char h[5]={0};
  f.read((uint8_t*)h,4);
  if (strncmp(h,"RIFF",4)!=0) { Serial.println("Not RIFF"); f.close(); return; }
  f.seek(8);
  f.read((uint8_t*)h,4);
  if (strncmp(h,"WAVE",4)!=0) { Serial.println("Not WAVE"); f.close(); return; }
  if (!skipToDataChunk(f)) { Serial.println("No data"); f.close(); return; }

  int16_t mono[256];
  int16_t stereo[512];

  while (true) {
    int bytes = f.read((uint8_t*)mono, sizeof(mono));
    if (bytes <= 0) break;
    int samples = bytes / 2;

    for (int i=0;i<samples;i++){
      int32_t s = mono[i];
      s *= DIGITAL_GAIN;
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;
      stereo[2*i]   = (int16_t)s;
      stereo[2*i+1] = (int16_t)s;
    }

    size_t w = 0;
    i2s_write(I2S_NUM_0, stereo, samples * 4, &w, portMAX_DELAY);
  }

  f.close();
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ================= MAIN =================
bool debugOn = false;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // wake MPUs
  mpuWrite(MPU_L, 0x6B, 0x00);
  mpuWrite(MPU_R, 0x6B, 0x00);

  Serial.println("Mount LittleFS...");
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS FAIL");
    while(true) delay(100);
  }
  Serial.println("LittleFS OK");

  i2sInit();

  if (loadCal()) {
    calibrated = true;
    Serial.println("CAL: loaded from NVS");
  } else {
    calibrateZero();
  }

  Serial.println("READY");
  Serial.println("Serial: c=calibrate+save, d=debug on/off");
}

void loop() {
  // serial commands
  if (Serial.available()) {
    char c = Serial.read();
    if (c=='c' || c=='C') calibrateZero();
    if (c=='d' || c=='D') { debugOn = !debugOn; Serial.println(debugOn ? "DEBUG ON" : "DEBUG OFF"); }
  }

  if (!calibrated) { delay(50); return; }

  static float fpL=0, frL=0, fpR=0, frR=0;

  MpuData L,R;
  bool okL = readMpu(MPU_L, L);
  bool okR = readMpu(MPU_R, R);
  if(!okL || !okR) { delay(10); return; }

  applyHandFixes(L, R);

  fpL = ema(fpL, L.pitch);  frL = ema(frL, L.roll);
  fpR = ema(fpR, R.pitch);  frR = ema(frR, R.roll);

  float dpR = angleDiff(fpR, p0R); // правая: pitch delta
  float drL = angleDiff(frL, r0L); // левая: roll  delta

  Gest gR = gestRightFromPitch(dpR);
  Gest gL = gestLeftFromRoll(drL);

  int phRaw = phraseForOneAxis(dpR, drL, gR, gL);

  // ---- стабилизация ----
  static int lastRaw = 0;
  static int cnt = 0;
  if (phRaw == lastRaw) cnt++;
  else { lastRaw = phRaw; cnt = 0; }
  int phStable = (cnt >= STABLE_FRAMES) ? phRaw : 0;

  // ---- debug ----
  static uint32_t dbg=0;
  if (debugOn && millis()-dbg > 200) {
    dbg = millis();
    Serial.printf("dpR=%.1f gR=%d | drL=%.1f gL=%d | raw=%d stab=%d\n",
                  dpR, (int)gR, drL, (int)gL, phRaw, phStable);
  }

  // ---- один звук на один жест (пока не вернёшься в NEU) ----
  static bool armed = true;
  static uint32_t neuSince = 0;

  if (phStable == 0) {
    if (neuSince == 0) neuSince = millis();
    if (millis() - neuSince >= NEU_HOLD_MS) armed = true;
  } else {
    neuSince = 0;
  }

  if (armed && phStable > 0 && !playing) {
    armed = false;
    const char* fn = wavForPhrase(phStable);
    Serial.print("PLAY "); Serial.println(fn);
    playing = true;
    playWav(fn);
    playing = false;
  }

  delay(20);
}
