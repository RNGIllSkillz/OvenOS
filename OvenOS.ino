// ============================================================
//  CGF PID Oven Controller — OvenOS v2.5 
// ============================================================
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "types.h"
#include "state.h"    
#include "profiles.h"
#include "hardware.h"
#include "settings.h"
#include "api.h"

// --- Global Variable Definitions ---
MAX6675 thermocouple(PIN_TC_CLK, PIN_TC_CS, PIN_TC_DO);
double Setpoint = 0, Input = 25, Output = 0;
PID myPID(&Input, &Output, &Setpoint, PID_KP, PID_KI, PID_KD, DIRECT);

AsyncWebServer server(80); 
portMUX_TYPE ssrmux = portMUX_INITIALIZER_UNLOCKED;

volatile bool running = false;
volatile bool timerActive = false;
volatile bool finished = false;
volatile bool emergencyStopped = false;
volatile bool monitoring = false;

bool profMode = false;
int profStep = -1;
char statusMsg[64] = "Ожидание";

const Profile* activeProfilePtr = nullptr;
Profile customProfile = { "Custom", 0, {} };
char customName[32] = "Custom";
volatile bool hasCustomProfile = false;

uint32_t runStart = 0;
uint32_t stabStart = 0;
uint32_t procStart = 0;
unsigned long holdMin = 0;
volatile uint32_t ssrDeadmanKick = 0;
uint32_t heatStartTime = 0;
double lastValidTemp = 25.0;
int tcFailCount = 0;

HistoryBuffer history = {};
uint32_t lastHistCapture = 0;
uint32_t windowStartTime = 0;

// Safety State
double currentStepPeak = 0.0;

// Mutex Definition
SemaphoreHandle_t dataMutex;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[INIT] OvenOS v2.5");

    // Create Mutex with Assert
    dataMutex = xSemaphoreCreateMutex();
    configASSERT(dataMutex); 

    pinMode(PIN_SSR, OUTPUT); 
    digitalWrite(PIN_SSR, LOW);

    if (!LittleFS.begin(true)) {
        Serial.println("[WARN] LittleFS fail");
    } else {
        loadCustomProfile();
        loadSettings();
    }

    // --- WIFI SETUP  ---
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    uint32_t wifiStart = millis();
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
        delay(100); 
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[INIT] IP: "); Serial.println(WiFi.localIP());
    }
    else {
        Serial.println("Connection failed");
    }

    myPID.SetOutputLimits(0, PID_WINDOW_SIZE);
    myPID.SetMode(MANUAL);
    myPID.SetSampleTime(250);

    registerAPI();
    server.begin();

    // Watchdog Setup (ESP32 Core 3.x compatible)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = 0, // 0 = Watch only loopTask
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdt_config);
    esp_task_wdt_add(NULL);

    // Deadman Timer
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &onDeadmanTimer;
    timer_args.name = "deadman";
    
    esp_timer_handle_t deadman_timer;
    esp_timer_create(&timer_args, &deadman_timer);
    esp_timer_start_periodic(deadman_timer, 500000); 
    
    ssrDeadmanKick = millis();
    Serial.println("[INIT] Ready");
}

void loop() {
    uint32_t now = millis();
    esp_task_wdt_reset();
    if (!emergencyStopped) {
        ssrDeadmanKick = now;
    }
    
    // WiFi Reconnection 
    static uint32_t lastWifiCheck = 0;
    if (now - lastWifiCheck > 30000) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WIFI] Connection lost. Reconnecting...");
            WiFi.reconnect(); 
        }
    }
    
    static uint32_t lastControl = 0;
    if (now - lastControl >= 250) {
        lastControl = now;
        updateThermocouple();
        runControlLoop();
    }

    delay(1); 
}