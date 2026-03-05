#pragma once
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include "language.h"
#include "state.h"
#include "profiles.h"
#include "webpage.h"
#include "favicon.h"

void sendSafeResponse(AsyncWebServerRequest *request, int code, const String& content) {
    AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", content);
    response->addHeader("Connection", "close");
    request->send(response);
}

void registerAPI() {
    server.on("/", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", PAGE_HTML);
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/style.css", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", PAGE_CSS);
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/script.js", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", PAGE_JS);
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/lang.js", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", WEB_LANG_JS);
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/favicon.ico", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", (const uint8_t*)favicon_ico, favicon_ico_len);
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/status", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        char buf[512];
        char msgBuf[64] = "busy";
        char profNameBuf[33] = "";
        long rssi = WiFi.RSSI(); 
        
        double cInput = 0, cInput2 = 0, cSetpoint = 0, cKp = 0, cKi = 0, cKd = 0;
        int cStep = -1, cSteps = 0;
        unsigned long cHold = 0;
        uint32_t cProcStart = 0, cRunStart = 0;
        bool isRunning = false, isTimerActive = false, isEstop = false, isMon = false, cHasTC2 = false, cFan = false;
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            cInput = Input;
            cInput2 = Input2;
            cSetpoint = Setpoint;
            cStep = profStep;
            cHold = holdMin;
            cProcStart = procStart;
            cRunStart = runStart;
            isRunning = running;
            isTimerActive = timerActive;
            isEstop = emergencyStopped;
            isMon = monitoring;
            cHasTC2 = hasTC2;
            cFan = fanState;
            cKp = PID_KP; cKi = PID_KI; cKd = PID_KD;
            
            strncpy(msgBuf, statusMsg, sizeof(msgBuf)-1);
            if (activeProfilePtr && profMode) {
                strncpy(profNameBuf, activeProfilePtr->name, 32);
                cSteps = activeProfilePtr->numSteps;
            }
            xSemaphoreGive(dataMutex);
        } else {
            sendSafeResponse(request, 503, "Busy");
            return;
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
            "{\"temp\":%.1f,\"temp2\":%.1f,\"hasTC2\":%s,\"fan\":%s,\"setpoint\":%.1f,\"msg\":\"%s\","
            "\"running\":%s,\"monitoring\":%s,\"profStep\":%d,\"profSteps\":%d,"
            "\"profName\":\"%s\",\"timeLeft\":%ld,\"holdMin\":%d,"
            "\"elapsed\":%lu,\"emergency\":%s,\"rssi\":%ld,"
            "\"kp\":%.2f,\"ki\":%.3f,\"kd\":%.3f}", 
            cInput, cInput2, cHasTC2 ? "true" : "false", cFan ? "true" : "false", cSetpoint, msgBuf,
            isRunning ? "true" : "false",
            isMon ? "true" : "false",
            cStep, cSteps, profNameBuf, timeLeft, holdMinV, 
            (millis() - cRunStart) / 1000UL,
            isEstop ? "true" : "false",
            rssi, cKp, cKi, cKd 
        );
        
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(buf));
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/history", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        uint16_t cnt = 0;
        uint16_t head = 0;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            cnt = history.count;
            head = history.head;
            xSemaphoreGive(dataMutex);
        } else {
            sendSafeResponse(request, 503, "Busy");
            return;
        }

        if (cnt == 0) {
            AsyncWebServerResponse *response = request->beginResponse(200, "application/octet-stream", (uint8_t*)nullptr, 0);
            response->addHeader("Connection", "close");
            request->send(response);
            return;
        }

        size_t totalLen = 2 + (cnt * 6);

        // Calculates bytes linearly from circular buffer directly into TCP packets!
        AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", totalLen, 
            [cnt, head](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                size_t bytesWritten = 0;
                
                // Grab lock briefly just to copy the necessary bytes for this packet
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) != pdTRUE) return 0;

                if (index < 2) {
                    size_t toWrite = (maxLen > (2 - index)) ? (2 - index) : maxLen;
                    memcpy(buffer, ((uint8_t*)&cnt) + index, toWrite);
                    bytesWritten += toWrite;
                    buffer += toWrite;
                    maxLen -= toWrite;
                    index += toWrite;
                }

                int base = (head - cnt + HIST_SIZE) % HIST_SIZE;

                while (maxLen > 0 && index < (2 + (cnt * 6))) {
                    size_t dataIndex = index - 2;
                    size_t pointIndex = dataIndex / 6;
                    size_t byteOffset = dataIndex % 6;

                    if (pointIndex >= cnt) break;

                    int realIdx = (base + pointIndex) % HIST_SIZE;

                    uint8_t ptBuf[6];
                    memcpy(&ptBuf[0], (const void*)&history.ts_offsets[realIdx], 2);
                    memcpy(&ptBuf[2], (const void*)&history.temps[realIdx], 2);
                    memcpy(&ptBuf[4], (const void*)&history.sps[realIdx], 2);

                    size_t toWrite = (maxLen > (6 - byteOffset)) ? (6 - byteOffset) : maxLen;
                    memcpy(buffer, ptBuf + byteOffset, toWrite);

                    bytesWritten += toWrite;
                    buffer += toWrite;
                    maxLen -= toWrite;
                    index += toWrite;
                }

                xSemaphoreGive(dataMutex);
                return bytesWritten;
            });

        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/start", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }
        
        bool isEstop = false;
        portENTER_CRITICAL(&ssrmux);
        isEstop = emergencyStopped;
        portEXIT_CRITICAL(&ssrmux);

        if (isEstop) { sendSafeResponse(request, 409, "Emergency Stop Active"); return; }

        bool isRunning = false;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            isRunning = running;
            xSemaphoreGive(dataMutex);
        }

        if (isRunning) { sendSafeResponse(request, 409, "Already Running"); return; }
        
        String m = request->arg("mode");
        resetRunState(); 

        if (m == "profile") {
            int idx = request->arg("profile").toInt();
            const Profile* selectedProfile = nullptr;
            if (idx == NUM_BUILTIN) {
                if (!hasCustomProfile) { sendSafeResponse(request, 400, "No Profile"); return; }
                selectedProfile = &customProfile;
            } else if (idx >= 0 && idx < NUM_BUILTIN) {
                selectedProfile = &BUILTIN_PROFILES[idx];
            } else {
                sendSafeResponse(request, 400, "Bad Index"); return;
            }
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                activeProfilePtr = selectedProfile;
                profMode = true;
                xSemaphoreGive(dataMutex);
            } else {
                sendSafeResponse(request, 503, "Busy"); return;
            }
            
            beginStep(0); 
            sendSafeResponse(request, 200, "OK");

        } else if (m == "manual") {
            double t = request->arg("temp").toDouble();
            int tm = request->arg("time").toInt();
            
            if (t < 20.0 || t > SAFETY_MAX_TEMP || tm < 1 || tm > 600) { 
                sendSafeResponse(request, 400, "Out of Bounds"); return; 
            }
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                profMode = false;
                activeProfilePtr = nullptr;
                Setpoint = t;
                holdMin = (unsigned long)tm; 
                
                timerActive = false;
                stabStart = 0;
                procStart = 0;
                heatStartTime = 0;
                finished = false;
                profStep = -1;
                
                stepStartTemp = Input;    
                currentStepPeak = Input;  
                
                running = true;
                strncpy(statusMsg, L_STATE_HEATING, sizeof(statusMsg)-1);
                myPID.SetMode(AUTOMATIC);
                xSemaphoreGive(dataMutex);
                sendSafeResponse(request, 200, "OK");
            } else {
                sendSafeResponse(request, 503, "Busy");
            }
        } else {
            sendSafeResponse(request, 400, "Bad Mode");
        }
    });

    server.on("/stop", HTTP_POST,[](AsyncWebServerRequest *request){        
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }
        stopReflow();
        sendSafeResponse(request, 200, "OK");
    });

    server.on("/reset", HTTP_POST,[](AsyncWebServerRequest *request){        
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }

        portENTER_CRITICAL(&ssrmux);
        emergencyStopped = false;
        digitalWrite(PIN_SSR, LOW);
        portEXIT_CRITICAL(&ssrmux);
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            running = false;
            finished = false;
            timerActive = false;
            tcFailCount = 0;
            tcVerifyPending = false;
            strncpy(statusMsg, L_STATE_WAIT, sizeof(statusMsg)-1);
            xSemaphoreGive(dataMutex);
        }
        ssrDeadmanKick = millis();
        sendSafeResponse(request, 200, "OK");
    });

    server.on("/profiles", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        // Use String to build response cleanly without AsyncResponseStream fragments
        String json;
        json.reserve(1536);
        json += "[";
        for (int i = 0; i < NUM_BUILTIN; i++) {
            const Profile& p = BUILTIN_PROFILES[i];
            char buf[128];
            snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"steps\":[", p.name);
            json += buf;
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
        
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/getcustom", HTTP_GET,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        
        if (LittleFS.exists("/profile.json")) {
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/profile.json", "application/json");
            response->addHeader("Connection", "close");
            request->send(response);
        } else {
            sendSafeResponse(request, 200, "null");
        }
    });

    AsyncCallbackJsonWebHandler* customHandler = new AsyncCallbackJsonWebHandler("/setcustom",[](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }

        bool isRunningCustom = false;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            isRunningCustom = (running && activeProfilePtr == &customProfile);
            xSemaphoreGive(dataMutex);
        }

        if (isRunningCustom) { sendSafeResponse(request, 409, "Cannot edit running profile"); return; }

        JsonObject doc = json.as<JsonObject>();
        if (doc.isNull()) { sendSafeResponse(request, 400, "Invalid JSON"); return; }

        const char* nameIn = doc["name"];
        if (!nameIn || strlen(nameIn) > 31) { sendSafeResponse(request, 400, "Invalid Name"); return; }

        JsonArray steps = doc["steps"];
        if (steps.size() == 0 || steps.size() > 8) { sendSafeResponse(request, 400, "Invalid Steps Count"); return; }

        ProfileStep tempSteps[8];
        int count = 0;
        for (JsonObject s : steps) {
            if (!s.containsKey("temp") || !s.containsKey("hold")) { sendSafeResponse(request, 400, "Missing temp/hold"); return; }
            
            double temp = s["temp"].as<double>();
            unsigned long hold = s["hold"].as<unsigned long>();
            
            if (temp < 0 || temp > 280 || hold < 1 || hold > 600) { sendSafeResponse(request, 400, "Out of Bounds"); return; }
            tempSteps[count].targetTemp = temp;
            tempSteps[count].holdMin = hold;
            sanitizeStr(tempSteps[count].label, s["label"] | L_DEFAULT_STEP, 23);
            count++;
        }

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sanitizeStr(customName, nameIn, sizeof(customName));
            customProfile.name = customName;
            for (int k = 0; k < count; k++) customProfile.steps[k] = tempSteps[k];
            customProfile.numSteps = count;
            hasCustomProfile = true;
            xSemaphoreGive(dataMutex);
            
            saveCustomProfile();
            sendSafeResponse(request, 200, "OK");
        } else {
            sendSafeResponse(request, 503, "Busy");
        }
    }); 
    
    customHandler->setMaxContentLength(2048); 
    server.addHandler(customHandler);

    server.on("/setpid", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }
        
        double tkp = PID_KP, tki = PID_KI, tkd = PID_KD;
        if (request->hasArg("kp")) tkp = request->arg("kp").toDouble();
        if (request->hasArg("ki")) tki = request->arg("ki").toDouble();
        if (request->hasArg("kd")) tkd = request->arg("kd").toDouble();
        
        if (tkp < 0.0 || tkp > 500.0 || tki < 0.0 || tki > 50.0 || tkd < 0.0 || tkd > 500.0) {
            sendSafeResponse(request, 400, "PID bounds err"); return;
        }

        PID_KP = tkp; PID_KI = tki; PID_KD = tkd;
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            myPID.SetTunings(PID_KP, PID_KI, PID_KD);
            xSemaphoreGive(dataMutex);
        }
        saveSettings(); 
        sendSafeResponse(request, 200, "OK");
    });

    server.on("/monitor", HTTP_POST,[](AsyncWebServerRequest *request){        
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }
        
        bool state = request->arg("state") == "1";
        bool isRunning = false;
        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            isRunning = running;
            if (state && isRunning) {
                xSemaphoreGive(dataMutex);
                sendSafeResponse(request, 409, "Busy");
                return;
            }
            
            monitoring = state;
            if (state) {
                history.head = 0;
                history.count = 0;
                forceHistoryCapture = true;
            }
            xSemaphoreGive(dataMutex);
        } else {
            sendSafeResponse(request, 503, "Busy");
            return;
        }
        
        if (state) {
            runStart = millis(); 
            lastHistCapture = millis();
        }

        sendSafeResponse(request, 200, "OK");
    });

    server.on("/fan", HTTP_POST,[](AsyncWebServerRequest *request){
        if (!request->authenticate(WEB_USER, WEB_PASS)) return request->requestAuthentication();
        if (!request->hasHeader("X-Oven-Auth")) { sendSafeResponse(request, 403, "CSRF"); return; }
        
        bool state = request->arg("state") == "1";        
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            fanState = state;
            xSemaphoreGive(dataMutex);
        } else {
            sendSafeResponse(request, 503, "Busy");
            return;
        }

        portENTER_CRITICAL(&ssrmux);
        digitalWrite(PIN_FAN, state ? HIGH : LOW);
        portEXIT_CRITICAL(&ssrmux);
        
        sendSafeResponse(request, 200, "OK");
    });
}