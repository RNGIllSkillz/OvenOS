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
            profNameBuf[32] = '\0';
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

    server.on("/history", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        HistoryBuffer* snap = new (std::nothrow) HistoryBuffer();
        if (!snap) { request->send(500, "text/plain", "OOM"); return; }

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            *snap = history; 
            xSemaphoreGive(dataMutex);
        } 

        int cnt = snap->count;
        if (cnt == 0) {
            request->send(200, "application/octet-stream", (uint8_t*)nullptr, 0);
            delete snap;
            return;
        }

        AsyncResponseStream *response = request->beginResponseStream("application/octet-stream");
        
        // Write Header (2 bytes)
        uint16_t n = (uint16_t)cnt;
        response->write((uint8_t*)&n, 2);

        // Write points (6 bytes per point)
        int base = (snap->head - cnt + HIST_SIZE) % HIST_SIZE;
        for (int i = 0; i < cnt; i++) {
            int idx = (base + i) % HIST_SIZE;
            response->write((uint8_t*)&snap->ts_offsets[idx], 2);
            response->write((uint8_t*)&snap->temps[idx], 2);
            response->write((uint8_t*)&snap->sps[idx], 2);
        }
        
        request->send(response);
        delete snap;
    });

    server.on("/start", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
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
        stopReflow();
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            myPID.SetMode(MANUAL);
            xSemaphoreGive(dataMutex);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/reset", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
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
        
        if (request->hasArg("kp")) PID_KP = request->arg("kp").toDouble();
        if (request->hasArg("ki")) PID_KI = request->arg("ki").toDouble();
        if (request->hasArg("kd")) PID_KD = request->arg("kd").toDouble();
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            myPID.SetTunings(PID_KP, PID_KI, PID_KD);
            xSemaphoreGive(dataMutex);
        }
        saveSettings(); 
        
        request->send(200, "text/plain", "OK");
    });

    server.on("/monitor", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
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