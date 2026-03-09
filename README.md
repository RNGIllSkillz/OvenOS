# IllOven (IllOvenOS v3.1) 🌡️

A smart, responsive, web-enabled ESP32 temperature controller designed for reflow ovens, filament drying, and high-thermal-mass plastic annealing (e.g., sand baking). 

This firmware transforms a standard toaster oven or custom heating chamber into a precise thermal control device utilizing PID logic, real-time web charting, dual-thermocouple monitoring, and automated hardware safety systems.

![alt text](https://img.shields.io/badge/Web_UI-Russian-blue) 
![alt text](https://img.shields.io/badge/Web_UI-English-blue)
![alt text](https://img.shields.io/badge/Platform-ESP32-success)
![alt text](https://img.shields.io/badge/Framework-Arduino-orange)

## ✨ Features
* **Web-Based UI**: Modern, responsive interface served directly from the ESP32. No cloud needed.
* **Dual Thermocouple Support**: Uses TC1 for ambient/heater PID control, and an optional TC2 to measure the actual board/part temperature to accurately trigger hold timers and render thermal lag.
* **Live & Historical Charting**: Real-time 3-line graphing (Air, Target, and Part temperatures), with up to 10 hours of historical data retention and PNG export.
* **Custom & Built-in Profiles**: Pre-loaded profiles for Reflow Soldering, ABS Annealing, and drying various filaments (PLA, PETG, Nylon, PC, PEEK). Create and save custom profiles right in the browser.
* **Automated Electronics Cooling**: The fan output actively monitors the oven's residual heat and the ESP32's internal CPU temperature, automatically cooling the SSR and logic board even after a cycle finishes.
* **Safety First**: Features a hardware deadman-timer, thermal runaway protection, max-temperature limits, thermal-drop trap, fail-safe fan states, and thermocouple failure detection.
* **Multi-Language**: Built-in English and Russian localizations.

## 🛠 Hardware Requirements
* **Microcontroller**: ESP32 (NodeMCU / DevKitC)
* **Temperature Sensors**: 1x or 2x MAX6675 Amplifiers with K-Type Thermocouples.
* **Heater Control**: Solid State Relay (SSR) rated for your heater's current.
* **Cooling Fan**: DC Fan controlled via a standard MOSFET/Relay module (used for cooling the electronics compartment).

## 📍 Pin Configuration (`config.h`)
| Component | ESP32 Pin | Note |
| :--- | :--- | :--- |
| SSR (Heater) | `GPIO 17` | Output to SSR |
| MAX6675 Clock | `GPIO 15` | Shared SCK |
| MAX6675 Data Out| `GPIO 4`  | Shared MISO / DO |
| TC1 Chip Select | `GPIO 2`  | Primary (Air/Heater) |
| TC2 Chip Select | `GPIO 16` | Secondary (Part/Sand) |
| Fan Control | `GPIO 27` | Output to Fan Relay/MOSFET |

## 🚀 Installation & Setup

1. **Clone the repository.**
2. **Set your WiFi credentials** in `credentials.h`:
   ```cpp
   #define WIFI_SSID "YourNetwork"
   #define WIFI_PASS "YourPassword"
   #define WIFI_AP_SSID "Oven-AP"
   #define WIFI_AP_PASS "12345678"
   #define WEB_USER "admin"
   #define WEB_PASS "admin"
Upload Filesystem: You must upload the LittleFS filesystem to the ESP32 (used for saving settings and custom profiles).
Compile & Flash: Upload the firmware to your ESP32.
Navigate: Open your browser to the IP address shown in the Serial Monitor (or the AP IP if your WiFi was unavailable).

🧰 Dependencies

This project requires the following libraries:
ESPAsyncWebServer (and AsyncTCP) by ESP32Async
ArduinoJson
PID_v1
max6675
