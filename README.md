OvenOS v3.0 (Temperature Controller)
OvenOS is an enterprise-grade, highly robust ESP32-based PID temperature controller firmware. It is designed to turn any generic toaster oven or dedicated heating chamber into a precision smart oven. It is perfect for PCB SMD reflow soldering, 3D printing filament drying, and plastic annealing.
Featuring a responsive, dark-mode web interface with live charting, strict RTOS-level thread safety, and multi-layered hardware/software thermal protections.

![alt text](https://img.shields.io/badge/Web_UI-Russian-blue) 
![alt text](https://img.shields.io/badge/Platform-ESP32-success)
![alt text](https://img.shields.io/badge/Framework-Arduino-orange)

Key Features
 1. Enterprise-Grade Safety Interlocks
Hardware Deadman Timer: An independent high-resolution esp_timer runs outside the main loop. If the core freezes, the SSR shuts off automatically.
Thermal Runaway Protection: Detects if the heater is stuck on without the temperature rising adequately.
Temperature Drop/Fault Detection: Triggers an E-Stop if the temperature unexpectedly plunges during a heating phase.
Thermocouple Validation: Advanced noise-filtering and disconnect detection. Safely halts if the sensor fails or unplugs.
Dual-Core Thread Safety: Mutex spinlocks (portENTER_CRITICAL) prevent race conditions between web requests and hardware timers.
Hardware Watchdog: ESP32 Task Watchdog Timer (WDT) enabled.
 2. Smart Web Interface
Real-time & Archive Graphing: Stores up to 10 hours of temperature history locally on the ESP32. Seamlessly syncs 60-second historical archives with a buttery-smooth 2-second real-time web graph.
Built-in & Custom Profiles: Ships with pre-configured profiles for SMD Reflow, ABS Annealing, and drying specific 3D printing filaments (PLA, PETG, Nylon/PA, PC, PEEK).
Profile Editor: Build, edit, and save your own multi-step heating/cooling profiles.
Live PID Tuning: Adjust Proportional, Integral, and Derivative constants on the fly without reflashing.
Secure: Protected by HTTP Basic Authentication.
AP Fallback: Automatically creates an Access Point (OvenOS_AP) if your home WiFi router goes offline.

Hardware Wiring
ESP32 Pin	Component	Description
GPIO 4	SSR (Solid State Relay)	Controls the heating element
GPIO 19	MAX6675 (DO / MISO)	Thermocouple Data Out
GPIO 5	MAX6675 (CS)	Thermocouple Chip Select
GPIO 18	MAX6675 (CLK / SCK)	Thermocouple Clock
Note: Pins can be easily modified in config.h.

Installation & Setup
1. Dependencies
You will need the following libraries installed in your Arduino IDE or PlatformIO:
ESPAsyncWebServer (ESP32Async)
AsyncTCP (ESP32Async)
ArduinoJson (v7.0.0 or higher required)
PID_v1 (by br3ttb)
MAX6675 (Adafruit or similar)
