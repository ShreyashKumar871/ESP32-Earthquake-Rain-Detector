# ESP32 Earthquake & Rain Detector 🌍🌧️

> **🏆 GeoLab 2026 - 2nd Prize Winner**
> This project was developed for and featured at GeoLab 2026, where it was awarded 2nd place for technical innovation and execution.

An ESP32-based environmental monitoring system that detects seismic activity (earthquakes) and rainfall. It acts as its own Wi-Fi Access Point (AP) and hosts a sleek, real-time web dashboard to visualize accelerometer shifts and rain sensor data without needing an external internet connection.

## Features

* **Real-time Seismograph Dashboard:** A modern HTML/JS web interface hosted entirely on the ESP32.
* **Independent Wi-Fi Network:** The ESP32 creates its own Access Point (`EarthquakeDetector`).
* **Motion Detection:** Uses an MPU6050 accelerometer to detect X, Y, and Z-axis shifts.
* **Rain Detection:** Integrates an MH-RD (FC-37) rain/moisture sensor to monitor weather conditions.
* **Audio-Visual Alarms:** Triggers a buzzer and blinking red LED during critical seismic events.
* **Test Mode:** Includes a web-based manual trigger to test the alarm system remotely.

## Hardware Requirements

* ESP32 Development Board
* MPU6050 Accelerometer/Gyroscope Module
* MH-RD / FC-37 Rain Sensor Module
* 1x Active Buzzer
* 3x LEDs (Red, Green, Blue)
* Jumper Wires & Breadboard

## Pin Configuration

| Component | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **Buzzer** | GPIO 25 | Active HIGH |
| **Green LED** | GPIO 26 | Status indicator (blinks softly when safe) |
| **Red LED** | GPIO 27 | Alarm indicator (blinks rapidly during quake) |
| **Blue LED** | GPIO 4 | Rain indicator (lights up when rain is detected) |
| **Rain Sensor (DO)**| GPIO 34 | Digital Output - Threshold comparator |
| **Rain Sensor (AO)**| GPIO 35 | Analog Output - Raw moisture reading |
| **MPU6050 (SDA)** | Default I2C SDA | Varies by ESP32 board (usually GPIO 21) |
| **MPU6050 (SCL)** | Default I2C SCL | Varies by ESP32 board (usually GPIO 22) |

## Software Dependencies

To compile and upload this code, you will need the Arduino IDE with the ESP32 board manager installed, along with the following libraries:
* `Wire.h` (Built-in)
* `WiFi.h` (Built-in)
* `WebServer.h` (Built-in)
* [I2Cdev](https://github.com/jrowberg/i2cdevlib)
* [MPU6050](https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/MPU6050)

## How to Use

1. **Flash the ESP32:** Upload the `.ino` file to your ESP32 using the Arduino IDE.
2. **Connect to the Network:** On your smartphone or laptop, search for available Wi-Fi networks and connect to **`EarthquakeDetector`** (Password: `quake1234`).
3. **Open the Dashboard:** Open a web browser and navigate to the IP address printed in your Arduino Serial Monitor (default is `http://192.168.4.1/`).
4. **Monitor:** Watch the live seismograph graphs and rain status. You can tap "Trigger Test Earthquake" to verify the alarms.

## Credits
* **Technical Head:** [Shreyash Kumar] 
* **Event:** GeoLab 2026 at Wadia College
