#pragma once
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "state.h"
#include "profiles.h"
#include "webpage.h"
#include "favicon.h"

static HistoryBuffer historySnapshot;

// Helper to check token for protected endpoints
bool checkToken(AsyncWebServerRequest *request) {
    if (request->hasArg("token") && request->arg("token") == API_TOKEN) {
        return true;
    }
    request->send(403, "text/plain", "Forbidden");
    return false;
}

void registerAPI() {
    // 1. ROOT PAGE
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", PAGE_HTML);
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/css", PAGE_CSS);
    });

    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "application/javascript", PAGE_JS);
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "image/x-icon", (const uint8_t*)favicon_ico, favicon_ico_len);
    });

    // 3. STATUS (JSON)
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        char buf[640]; 
        char msgBuf[64];
        char profNameBuf[33] = "";
        
        long rssi = WiFi.RSSI(); 
        
        bool isRunning = running;
        bool isTimerActive = timerActive;
        double cInput = Input;
        double cSetpoint = Setpoint;
        int cStep = profStep;
        unsigned long cHold = holdMin;
        uint32_t cProcStart = procStart;
        
        if (activeProfilePtr && profMode) {
            strncpy(profNameBuf, activeProfilePtr->name, 32);
        }

        long timeLeft = 0;
        int holdMinV = 0;
        
        if (isTimerActive && isRunning && cHold > 0) {
            uint32_t now = millis();
            unsigned long el = (now - cProcStart) / 1000;
            unsigned long tot = cHold * 60UL;
            timeLeft = (el < tot) ? (long)(tot - el) : 0L;
            holdMinV = (int)cHold;
        }

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            strncpy(msgBuf, statusMsg, sizeof(msgBuf)-1);
            xSemaphoreGive(dataMutex);
        } else {
            strncpy(msgBuf, "busy", sizeof(msgBuf)-1);
        }
        msgBuf[sizeof(msgBuf)-1] = 0;
        
        snprintf(buf, sizeof(buf),
            "{\"temp\":%.1f,\"setpoint\":%.1f,\"msg\":\"%s\","
            "\"running\":%s,\"monitoring\":%s,\"profStep\":%d,\"profSteps\":%d,"
            "\"profName\":\"%s\",\"timeLeft\":%ld,\"holdMin\":%d,"
            "\"elapsed\":%lu,\"emergency\":%s,\"rssi\":%ld,"
            "\"kp\":%.2f,\"ki\":%.3f,\"kd\":%.3f}", 
            cInput, cSetpoint, msgBuf,
            isRunning ? "true" : "false",
            monitoring ? "true" : "false",
            cStep, 
            (activeProfilePtr && profMode) ? activeProfilePtr->numSteps : 0,
            profNameBuf, timeLeft, holdMinV, 
            (millis() - runStart) / 1000UL,
            emergencyStopped ? "true" : "false",
            rssi,
            PID_KP, PID_KI, PID_KD 
        );
        
        request->send(200, "application/json", buf);
    });

    // 4. HISTORY 
    server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request){
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            historySnapshot = history; 
            xSemaphoreGive(dataMutex);
        } else {
            // If mutex is stuck (rare), we send the stale snapshot 
            // rather than failing or blocking indefinitely.
            // This is safer for the oven control loop.
        }

        AsyncResponseStream *response = request->beginResponseStream("application/json");
        
        response->print("{\"ts\":[");
        
        int cnt = historySnapshot.count;
        int base = (historySnapshot.head - cnt + HIST_SIZE) % HIST_SIZE;

        for (int i = 0; i < cnt; i++) {
            int idx = (base + i) % HIST_SIZE;
            response->printf("%u", historySnapshot.timestamps[idx]);
            if (i < cnt - 1) response->print(",");
        }

        response->print("],\"temps\":[");
        for (int i = 0; i < cnt; i++) {
            int idx = (base + i) % HIST_SIZE;
            response->printf("%.1f", historySnapshot.temps[idx]);
            if (i < cnt - 1) response->print(",");
        }

        response->print("],\"sps\":[");
        for (int i = 0; i < cnt; i++) {
            int idx = (base + i) % HIST_SIZE;
            response->printf("%.0f", historySnapshot.sps[idx]);
            if (i < cnt - 1) response->print(",");
        }
        
        response->print("]}");
        request->send(response);
    });

    // 5. START
    server.on("/start", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkToken(request)) return;
        
        if (emergencyStopped) {
            request->send(409, "text/plain", "Emergency Stop Active");
            return;
        }

        if (running) { 
            request->send(409, "text/plain", "Already Running"); 
            return; 
        }
        
        String m = request->arg("mode");
        resetRunState(); 

        if (m == "profile") {
            int idx = request->arg("profile").toInt();
            if (idx == NUM_BUILTIN) {
                if (!hasCustomProfile) { request->send(400); return; }
                activeProfilePtr = &customProfile;
            } else if (idx >= 0 && idx < NUM_BUILTIN) {
                activeProfilePtr = &BUILTIN_PROFILES[idx];
            } else {
                request->send(400); return;
            }
            profMode = true;
            profStep = 0;
            beginStep(0);
            running = true;
            myPID.SetMode(AUTOMATIC);
            request->send(200, "text/plain", "OK");

        } else if (m == "manual") {
            profMode = false;
            activeProfilePtr = nullptr;
            Setpoint = request->arg("temp").toDouble();
            holdMin = request->arg("time").toInt();
            running = true;
            myPID.SetMode(AUTOMATIC);
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                strncpy(statusMsg, "НАГРЕВ...", sizeof(statusMsg)-1);
                xSemaphoreGive(dataMutex);
            }
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400);
        }
    });

    // 6. STOP
    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkToken(request)) return;
        stopReflow();
        request->send(200, "text/plain", "OK");
    });

    // 7. RESET
    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkToken(request)) return;
        emergencyStopped = false;
        running = false;
        finished = false;
        timerActive = false;
        digitalWrite(PIN_SSR, LOW);
        ssrDeadmanKick = millis();
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            strncpy(statusMsg, "ОЖИДАНИЕ", sizeof(statusMsg)-1);
            xSemaphoreGive(dataMutex);
        }
        request->send(200, "text/plain", "OK");
    });

    // 7.b. Get List of Built-in Profiles
    server.on("/profiles", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print("[");
        
        for (int i = 0; i < NUM_BUILTIN; i++) {
            const Profile& p = BUILTIN_PROFILES[i];
            response->print("{\"name\":\"");
            response->print(p.name);
            response->print("\",\"steps\":[");
            for (int j = 0; j < p.numSteps; j++) {
                const ProfileStep& s = p.steps[j];
                response->printf("{\"l\":\"%s\",\"t\":%.0f,\"h\":%lu}", s.label, s.targetTemp, s.holdMin);
                if (j < p.numSteps - 1) response->print(",");
            }
            response->print("]}");
            if (i < NUM_BUILTIN - 1) response->print(",");
        }
        
        response->print("]");
        request->send(response);
    });

    // 8. GET CUSTOM PROFILE
    server.on("/getcustom", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/profile.json")) {
            request->send(LittleFS, "/profile.json", "application/json");
        } else {
            request->send(200, "application/json", "null");
        }
    });

    // 9. SET CUSTOM PROFILE
    server.on("/setcustom", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "OK");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            if (!checkToken(request)) return;

            DynamicJsonDocument doc(2048);
            DeserializationError err = deserializeJson(doc, (const char*)data, len);
            
            if (err) {
                Serial.println("JSON parse failed");
                return;
            }

            const char* nameIn = doc["name"];
            if (!nameIn || strlen(nameIn) > 31) return;

            JsonArray steps = doc["steps"];
            if (steps.size() == 0 || steps.size() > 8) return;

            ProfileStep tempSteps[8];
            int count = 0;
            for (JsonObject s : steps) {
                if (!s.containsKey("temp") || !s.containsKey("hold")) return;
                double temp = s["temp"];
                unsigned long hold = s["hold"];
                
                if (temp < 0 || temp > 280 || hold < 1 || hold > 600) return;
                tempSteps[count].targetTemp = temp;
                tempSteps[count].holdMin = hold;
                sanitizeStr(tempSteps[count].label, s["label"] | "Step", 23);
                count++;
            }

            sanitizeStr(customName, nameIn, sizeof(customName));
            customProfile.name = customName;
            for (int k = 0; k < count; k++) customProfile.steps[k] = tempSteps[k];
            customProfile.numSteps = count;
            hasCustomProfile = true;
            saveCustomProfile();
        }
    );

    // 10. SET PID
    server.on("/setpid", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkToken(request)) return;
        
        if (request->hasArg("kp")) PID_KP = request->arg("kp").toDouble();
        if (request->hasArg("ki")) PID_KI = request->arg("ki").toDouble();
        if (request->hasArg("kd")) PID_KD = request->arg("kd").toDouble();
        
        myPID.SetTunings(PID_KP, PID_KI, PID_KD);
        saveSettings(); 
        
        request->send(200, "text/plain", "OK");
    });

    // 11. TOGGLE MONITORING
    server.on("/monitor", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!checkToken(request)) return;
        
        bool state = request->arg("state") == "1";
        
        // Only allow enabling monitor if not running
        if (state && running) {
            request->send(409, "text/plain", "Busy");
            return;
        }

        monitoring = state;

        // If starting monitor, reset history for a clean graph
        if (state) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                history.head = 0;
                history.count = 0;
                xSemaphoreGive(dataMutex);
            }
            runStart = millis(); // Reset time base for graph
            lastHistCapture = millis();
        }

        request->send(200, "text/plain", "OK");
    });
}