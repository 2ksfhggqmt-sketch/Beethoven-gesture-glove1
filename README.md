# Beethoven Gesture Glove

A wearable gesture-based device that converts simple hand gestures into spoken audio phrases.  
The project is designed as an assistive communication tool for people with speech impairments.

## Team
**Team name:** Mozart  
**Participants:**  
- Tomiris Jaxylykova
- Dana Zhangabay  
- Begaiym Orazmbet 

**Country:** Kazakhstan  
**Competition:** INFOMATRIX-ASIA  
**Section:** Hardware Control  

---

## Project Description
Mozart Gesture Glove is a wearable hardware project that recognizes hand gestures using inertial sensors and converts them into pre-recorded voice messages.  
The system works fully offline and does not require cameras, internet connection, or external computing devices.

The main goal of the project is to create a simple, low-cost, and portable assistive communication solution.

---

## How It Works
- Two MPU6050 sensors are mounted on wearable gloves
- Each sensor measures the orientation of the hand in space
- Pitch and roll angles are calculated on the ESP32 microcontroller
- Gestures are detected using fixed threshold values
- Each recognized gesture triggers a corresponding pre-recorded WAV audio file
- Audio is played through an I2S amplifier and speaker

---

## Hardware Components
- ESP32 microcontroller  
- 2 × MPU6050 (I2C, addresses 0x68 and 0x69)  
- MAX98357A I2S audio amplifier  
- 8Ω / 0.25W speaker  
- 18650 Li-ion battery  
- TP4056 charging and protection module  
- MT3608 DC-DC boost converter  
- Resistors, capacitors, wiring  

---

## Gesture Logic
**Right Hand (Pitch axis):**
- UP → Phrase 1  
- DOWN → Phrase 2  
- STRONG UP → Phrase 3  

**Left Hand (Roll axis):**
- LEFT → Phrase 4  
- RIGHT → Phrase 5  
- STRONG RIGHT → Phrase 6  

Only one hand is active at a time to prevent false triggers.

---

## Software
- Language: C++  
- Framework: Arduino (ESP32)  
- Gesture processing using calibrated reference angles
- Fixed thresholds for stable gesture recognition
- WAV audio playback via I2S interface
- Audio files stored in LittleFS

---

## Audio Files
- Format: WAV  
- Sample rate: 8000 Hz  
- Bit depth: 16-bit  
- Mono  
- Stored in LittleFS memory  

File naming example:
p1.wav, p2.wav, p3.wav, p4.wav, p5.wav, p6.wav

---

## Power System
- 18650 battery as main power source  
- TP4056 provides safe charging and battery protection  
- MT3608 boosts voltage to stable 5V  
- ESP32 and MAX98357A powered from boosted output  

---

## Current Limitations
- Limited number of gestures
- No finger-level gesture recognition
- Wired connection between gloves and control unit
- Uses pre-recorded audio only

---

## Future Development
- Multi-language support (Kazakh, Russian, English, Spanish, ASL)
- Finger position detection using flex or EMG sensors
- AI-based gesture recognition instead of fixed thresholds
- More compact design: ring + bracelet instead of gloves
- Built-in AI model to recognize gestures automatically and generate speech output

---

## Inspiration
This project is inspired by gesture-based wearable interfaces developed by research groups such as MIT Media Lab.  
All hardware design and software implementation were developed independently by the team.

---

## How to Run
1. Flash the firmware to ESP32  
2. Upload WAV files to LittleFS  
3. Power the device  
4. Calibrate by sending `c` via Serial Monitor  
5. Perform gestures to play audio phrases  

---

## License
This project is intended for educational and research purposes.

