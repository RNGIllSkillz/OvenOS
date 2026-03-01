#pragma once
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "language.h"
#include "state.h"
#include "profiles.h"
#include "webpage.h"
#include "favicon.h"

void registerAPI() {
    server.on("/", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        request->send_P(200, "text/html", PAGE_HTML);
    });

    server.on("/style.css", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        request->send_P(200, "text/css", PAGE_CSS);
    });

    server.on("/script.js", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        request->send_P(200, "application/javascript", PAGE_JS);
    });

    server.on("/lang.js", HTTP_GET,[](AsyncWebServerRequest *request){
        request->send_P(200, "application/javascript", WEB_LANG_JS);
    });

    server.on("/favicon.ico", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        request->send_P(200, "image/x-icon", (const uint8_t*)favicon_ico, favicon_ico_len);
    });

    server.on("/status", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        char buf[1024];
        char msgBuf[64] = "busy";
        char profNameBuf[33] = "";
        long rssi = WiFi.RSSI(); 
        
        // Snapshot variables safely
        double cInput = 0, cSetpoint = 0, cKp = 0, cKi = 0, cKd = 0;
        int cStep = -1, cSteps = 0;
        unsigned long cHold = 0;
        uint32_t cProcStart = 0, cRunStart = 0;
        bool isRunning = false, isTimerActive = false, isEstop = false, isMon = false;
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            cInput = Input;
            cSetpoint = Setpoint;
            cStep = profStep;
            cHold = holdMin;
            cProcStart = procStart;
            cRunStart = runStart;
            isRunning = running;
            isTimerActive = timerActive;
            isEstop = emergencyStopped;
            isMon = monitoring;
            cKp = PID_KP; cKi = PID_KI; cKd = PID_KD;
            
            strncpy(msgBuf, statusMsg, sizeof(msgBuf)-1);
            if (activeProfilePtr && profMode) {
                strncpy(profNameBuf, activeProfilePtr->name, 32);
                cSteps = activeProfilePtr->numSteps;
            }
            xSemaphoreGive(dataMutex);
        }
        msgBuf[sizeof(msgBuf)-1] = 0;
        profNameBuf[32] = 0;

        long timeLeft = 0;
        int holdMinV = (int)cHold;
        if (isTimerActive && isRunning && cHold > 0) {
            unsigned long el = (millis() - cProcStart) / 1000;
            unsigned long tot = cHold * 60UL;
            timeLeft = (el < tot) ? (long)(tot - el) : 0L;
        }
        
        snprintf(buf, sizeof(buf),
            "{\"temp\":%.1f,\"setpoint\":%.1f,\"msg\":\"%s\","
            "\"running\":%s,\"monitoring\":%s,\"profStep\":%d,\"profSteps\":%d,"
            "\"profName\":\"%s\",\"timeLeft\":%ld,\"holdMin\":%d,"
            "\"elapsed\":%lu,\"emergency\":%s,\"rssi\":%ld,"
            "\"kp\":%.2f,\"ki\":%.3f,\"kd\":%.3f}", 
            cInput, cSetpoint, msgBuf,
            isRunning ? "true" : "false",
            isMon ? "true" : "false",
            cStep, cSteps, profNameBuf, timeLeft, holdMinV, 
            (millis() - cRunStart) / 1000UL,
            isEstop ? "true" : "false",
            rssi, cKp, cKi, cKd 
        );
        request->send(200, "application/json", buf);
    });

    server.on("/history", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        static HistoryBuffer snap; 
        static volatile bool historyBusy = false;
        
        if (historyBusy) {
            request->send(503, "text/plain", "Busy");
            return;
        }
        historyBusy = true;
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            snap = history; 
            xSemaphoreGive(dataMutex);
        } 

        int cnt = snap.count;
        if (cnt == 0) {
            request->send(200, "application/octet-stream", (uint8_t*)nullptr, 0);
            historyBusy = false;
            return;
        }

        AsyncResponseStream *response = request->beginResponseStream("application/octet-stream");
        uint16_t n = (uint16_t)cnt;
        response->write((uint8_t*)&n, 2);

        int base = (snap.head - cnt + HIST_SIZE) % HIST_SIZE;
        for (int i = 0; i < cnt; i++) {
            int idx = (base + i) % HIST_SIZE;
            response->write((uint8_t*)&snap.ts_offsets[idx], 2);
            response->write((uint8_t*)&snap.temps[idx], 2);
            response->write((uint8_t*)&snap.sps[idx], 2);
        }
        
        request->send(response);
        historyBusy = false;
    });

    server.on("/start", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { request->send(403, "text/plain", "CSRF"); return; }
        
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
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                myPID.SetMode(AUTOMATIC);
                xSemaphoreGive(dataMutex);
            }
            request->send(200, "text/plain", "OK");

        } else if (m == "manual") {
            double t = request->arg("temp").toDouble();
            int tm = request->arg("time").toInt();
            
            if (t < 20.0 || t > SAFETY_MAX_TEMP || tm < 1 || tm > 600) { 
                request->send(400, "text/plain", "Out of Bounds"); 
                return; 
            }
            
            profMode = false;
            activeProfilePtr = nullptr;
            Setpoint = t;
            holdMin = (unsigned long)tm; // Safely cast positive bounds-checked int
            running = true;
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                strncpy(statusMsg, L_STATE_HEATING, sizeof(statusMsg)-1);
                myPID.SetMode(AUTOMATIC);
                xSemaphoreGive(dataMutex);
            }
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400);
        }
    });

    server.on("/stop", HTTP_POST,[](AsyncWebServerRequest *request){        
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { request->send(403, "text/plain", "CSRF"); return; }

        stopReflow();
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            myPID.SetMode(MANUAL);
            xSemaphoreGive(dataMutex);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/reset", HTTP_POST,[](AsyncWebServerRequest *request){        
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { request->send(403, "text/plain", "CSRF"); return; }

        portENTER_CRITICAL(&ssrmux);
        emergencyStopped = false;
        digitalWrite(PIN_SSR, LOW);
        portEXIT_CRITICAL(&ssrmux);
        
        running = false;
        finished = false;
        timerActive = false;
        tcFailCount = 0;
        tcVerifyPending = false;
        ssrDeadmanKick = millis();
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            strncpy(statusMsg, L_STATE_WAIT, sizeof(statusMsg)-1);
            xSemaphoreGive(dataMutex);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/profiles", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        String json;
        json.reserve(2048);
        json = "[";
        
        char buf[128];
        for (int i = 0; i < NUM_BUILTIN; i++) {
            const Profile& p = BUILTIN_PROFILES[i];
            json += "{\"name\":\"";
            json += p.name;
            json += "\",\"steps\":[";
            for (int j = 0; j < p.numSteps; j++) {
                const ProfileStep& s = p.steps[j];
                snprintf(buf, sizeof(buf), "{\"l\":\"%s\",\"t\":%.0f,\"h\":%lu}", s.label, s.targetTemp, s.holdMin);
                json += buf;
                if (j < p.numSteps - 1) json += ',';
            }
            json += "]}";
            if (i < NUM_BUILTIN - 1) json += ',';
        }
        
        json += "]";
        request->send(200, "application/json", json);
    });

    server.on("/getcustom", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        if (LittleFS.exists("/profile.json")) {
            request->send(LittleFS, "/profile.json", "application/json");
        } else {
            request->send(200, "application/json", "null");
        }
    });

    AsyncCallbackJsonWebHandler* customHandler = new AsyncCallbackJsonWebHandler("/setcustom",[](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { request->send(403, "text/plain", "CSRF"); return; }

        if (running && activeProfilePtr == &customProfile) {
            request->send(409, "text/plain", "Cannot edit running profile");
            return;
        }

        JsonObject doc = json.as<JsonObject>();
        if (doc.isNull()) { request->send(400, "text/plain", "Invalid JSON"); return; }

        const char* nameIn = doc["name"];
        if (!nameIn || strlen(nameIn) > 31) { request->send(400, "text/plain", "Invalid Name"); return; }

        JsonArray steps = doc["steps"];
        if (steps.size() == 0 || steps.size() > 8) { request->send(400, "text/plain", "Invalid Steps Count"); return; }

        ProfileStep tempSteps[8];
        int count = 0;
        for (JsonObject s : steps) {
            if (!s.containsKey("temp") || !s.containsKey("hold")) { request->send(400, "text/plain", "Missing temp/hold"); return; }
            double temp = s["temp"];
            unsigned long hold = s["hold"];
            
            if (temp < 0 || temp > 280 || hold < 1 || hold > 600) { request->send(400, "text/plain", "Out of Bounds"); return; }
            tempSteps[count].targetTemp = temp;
            tempSteps[count].holdMin = hold;
            sanitizeStr(tempSteps[count].label, s["label"] | L_DEFAULT_STEP, 23);
            count++;
        }

        sanitizeStr(customName, nameIn, sizeof(customName));
        customProfile.name = customName;
        for (int k = 0; k < count; k++) customProfile.steps[k] = tempSteps[k];
        customProfile.numSteps = count;
        hasCustomProfile = true;
        saveCustomProfile();

        request->send(200, "text/plain", "OK");
    }); 
    
    customHandler->setMaxContentLength(2048); 
    server.addHandler(customHandler);

    server.on("/setpid", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { request->send(403, "text/plain", "CSRF"); return; }
        
        double tkp = PID_KP, tki = PID_KI, tkd = PID_KD;
        if (request->hasArg("kp")) tkp = request->arg("kp").toDouble();
        if (request->hasArg("ki")) tki = request->arg("ki").toDouble();
        if (request->hasArg("kd")) tkd = request->arg("kd").toDouble();
        
        // Bounds checking: prevent dangerous values that would destabilize the control loop
        if (tkp < 0.0 || tkp > 500.0 || tki < 0.0 || tki > 50.0 || tkd < 0.0 || tkd > 500.0) {
            request->send(400, "text/plain", "PID values out of bounds");
            return;
        }

        PID_KP = tkp;
        PID_KI = tki;
        PID_KD = tkd;
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            myPID.SetTunings(PID_KP, PID_KI, PID_KD);
            xSemaphoreGive(dataMutex);
        }
        saveSettings(); 
        
        request->send(200, "text/plain", "OK");
    });

    server.on("/monitor", HTTP_POST,[](AsyncWebServerRequest *request){        
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { request->send(403, "text/plain", "CSRF"); return; }
        
        bool state = request->arg("state") == "1";
        
        if (state && running) {
            request->send(409, "text/plain", "Busy");
            return;
        }

        monitoring = state;

        if (state) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                history.head = 0;
                history.count = 0;
                xSemaphoreGive(dataMutex);
            }
            runStart = millis(); 
            lastHistCapture = millis();
            forceHistoryCapture = true;
        }

        request->send(200, "text/plain", "OK");
    });
}