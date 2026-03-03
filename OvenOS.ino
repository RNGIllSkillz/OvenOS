// ============================================================
//  IllOvenOS v3.0
// ============================================================
#include <WiFi.h>
#include <esp_wifi.h>
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
#include "language.h"

// --- Global Variable Definitions ---
double PID_KP = 80.0;
double PID_KI = 0.4;
double PID_KD = 1.5;

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
volatile bool tcVerifyPending = false;
volatile bool forceHistoryCapture = false;

bool profMode = false;
bool tcFirstRead = true;
int profStep = -1;
char statusMsg[64] = L_STATE_WAIT;

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
double stepStartTemp = 0.0;

SemaphoreHandle_t dataMutex;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[INIT] IllOvenOS v3.0");

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

    // --- WIFI SETUP WITH AP FALLBACK ---
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    esp_wifi_set_ps(WIFI_PS_NONE); 
    
    uint32_t wifiStart = millis();
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
        delay(100); 
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[INIT] STA IP: "); Serial.println(WiFi.localIP());
    }
    else {
        Serial.println("[WIFI] Connection failed. Starting AP Mode.");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS); 
        esp_wifi_set_ps(WIFI_PS_NONE);
        Serial.print("[INIT] AP IP: "); Serial.println(WiFi.softAPIP());
    }

    myPID.SetOutputLimits(0, PID_WINDOW_SIZE);
    myPID.SetMode(MANUAL);
    
    myPID.SetSampleTime(PID_WINDOW_SIZE);

    registerAPI();
    server.begin();

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = 0, 
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdt_config);
    esp_task_wdt_add(NULL);

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
    
    bool estopFlag;
    portENTER_CRITICAL(&ssrmux);
    estopFlag = emergencyStopped;
    portEXIT_CRITICAL(&ssrmux);

    if (!estopFlag) {
        ssrDeadmanKick = now;
    }
    
    static uint32_t lastControl = 0;
    if (now - lastControl >= PID_WINDOW_SIZE) {
        lastControl = now;
        updateThermocouple();
        runControlLoop();
    }

    uint32_t wElapsed = now - windowStartTime;
    while (wElapsed >= PID_WINDOW_SIZE) { 
        windowStartTime += PID_WINDOW_SIZE; 
        wElapsed -= PID_WINDOW_SIZE; 
    }

    bool safeToFire = false;
    double snapOutput = 0.0;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        safeToFire = running && !finished && !emergencyStopped 
                   && !tcVerifyPending && (tcFailCount < TC_FAIL_LIMIT);  
        snapOutput = Output;
        xSemaphoreGive(dataMutex);
    }

    digitalWrite(PIN_SSR, safeToFire && (snapOutput > wElapsed));

    delay(1); 
}