# Mozart-gesture-glove1

## Project Description
Mozart Gesture Glove is a wearable assistive device that converts simple hand gestures into spoken audio phrases.  
The project is designed to help people with speech impairments communicate using a low-cost, offline, and portable solution.

The system uses inertial sensors to detect hand orientation and triggers pre-recorded voice messages through a speaker.

## Team
**Team name:** Mozart  
**Authors:**  
- Iskander Sensei  
- Jessica Clein  
- Tomiris Dzhaxylykova  

**Country:** Kazakhstan  
**Section:** Hardware Control (INFOMATRIX-ASIA)

## How It Works
- Two MPU6050 sensors are placed on wearable gloves
- Sensors measure hand orientation (pitch and roll)
- ESP32 processes sensor data using fixed thresholds
- Each recognized gesture triggers a pre-recorded WAV audio file
- Audio is played through an I2S amplifier (MAX98357A) and speaker

Gesture flow:
Hand movement → MPU6050 → ESP32 → Gesture logic → WAV file → Speaker

## Hardware Components
- ESP32
- 2 × MPU6050 (I2C addresses 0x68 and 0x69)
- MAX98357A I2S audio amplifier
- 8Ω 0.25W speaker
- 18650 Li-ion battery
- TP4056 charging module
- MT3608 DC-DC boost converter
- Resistors, capacitors, wiring

## Gesture Logic
**Right Hand (Pitch):**
- UP → Phrase 1
- DOWN → Phrase 2
- STRONG UP → Phrase 3

**Left Hand (Roll):**
- LEFT → Phrase 4
- RIGHT → Phrase 5
- STRONG RIGHT → Phrase 6

Only one hand is active at a time to avoid false triggers.

## Audio Files
- WAV format
- 8 kHz, 16-bit, mono
- Stored in LittleFS
- Files: `/p1.wav` … `/p6.wav`

## Software
- Language: C++
- Framework: Arduino (ESP32)
- Gesture recognition based on calibrated reference angles
- Uses exponential smoothing and fixed thresholds for stability
- Offline operation (no internet, no cloud, no camera)

## Calibration
1. Power on the device
2. Keep both hands in neutral position
3. Press **`c`** in the Serial Monitor
4. Calibration data is saved in ESP32 memory

## Future Improvements
- Multi-language support (ASL, Russian, Kazakh, Spanish, English audio)
- Finger-level gesture recognition using flex or EMG sensors
- Embedded AI model for gesture classification
- Redesign into a ring + bracelet form factor for better portability

## Inspiration
Inspired by gesture-based wearable research from MIT Media Lab,  
but implemented independently with original hardware, software logic, and system architecture.

## License
MIT License  
For educational and non-commercial use.
