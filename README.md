# Cloud-Edge Gesture Recognition with ESP32 & Flask

## 🔧 Project Overview
A hybrid edge-cloud system that uses an ESP32 with MPU6050 sensor to detect gestures. It runs inference locally, and sends data to a Flask server only when confidence is low.

## 🧠 System Flow
- Local inference via Edge Impulse model on ESP32
- If confidence < threshold, ESP32 sends sensor data to cloud
- Flask server returns predicted gesture & confidence

## 📁 Project Structure
See folder breakdown in this repository.

## 🚀 How to Run
### ESP32
1. Open `ESP32_to_cloud.ino` in Arduino IDE.
2. Set WiFi credentials & server URL.
3. Upload to ESP32.

### Flask Server
```bash
cd app
pip install -r requirements.txt
python app.py
