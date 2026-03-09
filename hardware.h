#pragma once
#include "language.h"
#include "state.h"
#include "config.h"

uint32_t tcVerifyStartTime = 0;
double tcVerifyFirstReading = 0;
uint32_t fullPowerStartTime = 0;
const uint32_t RUNAWAY_TIMEOUT_MS = 180000; 
double runawayBaselineTemp = 0;

void onDeadmanTimer(void* arg) {
    if ((millis() - ssrDeadmanKick) > SSR_DEADMAN_MS) {
        portENTER_CRITICAL(&ssrmux);
        digitalWrite(PIN_SSR, LOW);
        digitalWrite(PIN_FAN, HIGH);
        fanState = true;   
        emergencyStopped = true; 
        portEXIT_CRITICAL(&ssrmux);
    }
}

void emergencyStop(const char* reason) {
    portENTER_CRITICAL(&ssrmux);
    digitalWrite(PIN_SSR, LOW);
    emergencyStopped = true;
    portEXIT_CRITICAL(&ssrmux);
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        running = false;
        timerActive = false;
        finished = false;
        profMode = false;
        activeProfilePtr = nullptr;
        profStep = -1;
        Output = 0;
        tcVerifyPending = false;
        myPID.SetMode(MANUAL);
        snprintf(statusMsg, sizeof(statusMsg), "%s%s", L_ERR_PREFIX, reason);
        xSemaphoreGive(dataMutex);
    }
    Serial.printf("!!! EMERGENCY STOP: %s !!!\n", reason);
}

void stopReflow() {
    portENTER_CRITICAL(&ssrmux);
    digitalWrite(PIN_SSR, LOW);
    portEXIT_CRITICAL(&ssrmux);

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        running = false;
        timerActive = false;
        finished = false;
        profMode = false;
        activeProfilePtr = nullptr;
        profStep = -1;
        Output = 0;
        tcFailCount = 0; 
        tcVerifyPending = false;
        myPID.SetMode(MANUAL);
        runawayBaselineTemp = 0;
        fullPowerStartTime = 0;
        strncpy(statusMsg, L_STATE_STOPPED, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        xSemaphoreGive(dataMutex);
    }
}

void beginStep(int step) {
    if (!activeProfilePtr || step < 0 || step >= activeProfilePtr->numSteps) {
        emergencyStop(L_ERR_INVALID_STEP);
        return;
    }
    const ProfileStep& s = activeProfilePtr->steps[step];
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        Setpoint = s.targetTemp;
        holdMin = s.holdMin;
        strncpy(statusMsg, s.label, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        
        timerActive = false;
        stabStart = 0;
        procStart = 0;
        heatStartTime = 0;
        finished = false;
        currentStepPeak = Input; 
        stepStartTemp = Input; 
        profStep = step;
        
        running = true;
        myPID.SetMode(AUTOMATIC);
        xSemaphoreGive(dataMutex);
    }
}

void resetRunState() {    
    portENTER_CRITICAL(&ssrmux);
    digitalWrite(PIN_SSR, LOW);
    portEXIT_CRITICAL(&ssrmux);

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        running = false;
        monitoring = false; 
        timerActive = false;
        finished = false;
        profStep = -1;
        activeProfilePtr = nullptr;
        history.head = 0;
        history.count = 0;
        heatStartTime = 0;
        tcFailCount = 0;
        tcVerifyPending = false;
        stabStart = 0;
        Output = 0;
        fullPowerStartTime = 0; 
        currentStepPeak = 0; 
        stepStartTemp = 0;
        runawayBaselineTemp = 0; 
        tcFirstRead = true;
        
        myPID.SetMode(MANUAL);
        myPID.SetOutputLimits(0, PID_WINDOW_SIZE);
        xSemaphoreGive(dataMutex);
    }
    
    runStart = millis();
    lastHistCapture = runStart;
    windowStartTime = millis();
}

bool isValidTCReading(double temp) {
    if (isnan(temp)) return false;
    if (temp <= 0.01 || temp > 550.0) return false;
    return true;
}

void updateThermocouple() {
    static uint32_t lastRead = 0;
    uint32_t now = millis();
    
    if (now - lastRead < 250) return;
    lastRead = now;

    if (tcVerifyPending) {
        if (now - tcVerifyStartTime < 250) return; 
        double v2 = thermocouple.readCelsius();
        tcVerifyPending = false;
        
        if (!isValidTCReading(v2)) {
             tcFailCount++;
             return;
        }
        
        if (fabs(v2 - tcVerifyFirstReading) < 10.0) {
             if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                 Input = v2;
                 lastValidTemp = v2;
                 tcFailCount = 0;
                 xSemaphoreGive(dataMutex);
             }
        }
        return;
    }

    double v = thermocouple.readCelsius();
    double v2 = thermocouple2.readCelsius();

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (!isValidTCReading(v2)) {
            hasTC2 = false;
        } else {
            Input2 = hasTC2 ? ((Input2 * 0.7) + (v2 * 0.3)) : v2;
            hasTC2 = true;
        }
        xSemaphoreGive(dataMutex);
    }

    if (!isValidTCReading(v)) {
        tcFailCount++;
        if (tcFailCount >= TC_FAIL_LIMIT) {
            if (running) emergencyStop(L_ERR_TC_FAIL);
            else {
                 if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                     Input = 0; 
                     xSemaphoreGive(dataMutex);
                 }
                 tcFailCount = 0; 
            }
        }
        return;
    }

    if (tcFirstRead) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            Input = v;
            lastValidTemp = v;
            tcFirstRead = false;
            tcFailCount = 0;
            xSemaphoreGive(dataMutex);
        }
        return;
    }

    if (lastValidTemp > 0.01 && fabs(v - lastValidTemp) > MAX_RISE_PER_READ) {
        tcVerifyPending = true;
        tcVerifyStartTime = now;
        tcVerifyFirstReading = v;
        return;
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Input = (Input * 0.7) + (v * 0.3);
        lastValidTemp = v;
        tcFailCount = 0;
        xSemaphoreGive(dataMutex);
    }
}

void manageFan() {
    bool autoFan = false;
    
    // Grab a quick thread-safe reading of the oven air temperature (TC1)
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        
        // Rule 1: Fail-safe. If thermocouple is failing, reading 0 or disconnected -> Fan ON
        if (tcFailCount > 0 || isnan(Input) || Input <= 0.01) {
            autoFan = true;
        } 
        // Rule 2: Oven internal temp > 50C -> Fan ON
        else if (Input > 50.0 || temperatureRead() > 55.0) {
            autoFan = true;
        } 
        // Rule 3: Temp <= 50C -> Fan OFF
        else {
            autoFan = false;
        }

        // Apply state if it needs to change
        if (fanState != autoFan) {
            fanState = autoFan;
            portENTER_CRITICAL(&ssrmux);
            digitalWrite(PIN_FAN, autoFan ? HIGH : LOW);
            portEXIT_CRITICAL(&ssrmux);
        }
        xSemaphoreGive(dataMutex);
    }
}

void runControlLoop() {
    manageFan();
    uint32_t now = millis();

    double snapSP, snapInput, snapInput2, snapStepStart, snapPeak;
    unsigned long snapHold;
    bool snapTimerActive, snapHasTC2;
    int snapProfStep;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        snapSP = Setpoint;
        snapHold = holdMin;
        snapInput = Input;
        snapInput2 = Input2;
        snapHasTC2 = hasTC2;
        snapStepStart = stepStartTemp;
        snapPeak = currentStepPeak;
        snapTimerActive = timerActive;
        snapProfStep = profStep;
        xSemaphoreGive(dataMutex);
    } else { return; }
    
    if (snapInput > SAFETY_MAX_TEMP) { emergencyStop(L_ERR_MAX_TEMP); return; }
    if (tcVerifyPending) return;

    bool estopFlag = false;
    portENTER_CRITICAL(&ssrmux); estopFlag = emergencyStopped; portEXIT_CRITICAL(&ssrmux);

    if (estopFlag) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
             if (strncmp(statusMsg, L_ERR_PREFIX, strlen(L_ERR_PREFIX)) != 0) {
                 snprintf(statusMsg, sizeof(statusMsg), "%s%s", L_ERR_PREFIX, L_ERR_FAULT);
                 forceHistoryCapture = true;
             }
             xSemaphoreGive(dataMutex);
        }
        return; 
    }

    if (running) {
        double maxExpected = (snapSP > snapStepStart) ? snapSP : snapStepStart;
        if (snapInput > (maxExpected + 20.0) && snapInput > 60.0) {
            emergencyStop(L_ERR_OVERSHOOT); return;
        }
        if (snapSP >= snapStepStart) {
            if (snapInput > snapPeak) {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    currentStepPeak = snapInput; snapPeak = snapInput;
                    xSemaphoreGive(dataMutex);
                }
            }
            double effectivePeak = (snapPeak > snapSP) ? snapSP : snapPeak;
            if (snapInput < snapSP && snapPeak > 40 && snapInput < (effectivePeak - 15.0)) {
                emergencyStop(L_ERR_DROP); return;
            }
        }        
    }

    bool shouldCapture = false;
    if (forceHistoryCapture) {
        shouldCapture = true; forceHistoryCapture = false;
    } else if ((running || monitoring || snapInput > 25.0) && (now - lastHistCapture >= HIST_INTERVAL)) {
        shouldCapture = true;
    }
    
    if (shouldCapture) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            int idx = history.head;
            history.temps[idx] = (int16_t)round(Input * 10.0);
            history.temps2[idx] = (int16_t)round(Input2 * 10.0);
            history.sps[idx] = (int16_t)round(Setpoint);
            uint32_t elapsed = (now >= runStart) ? (now - runStart) / 1000 : 0;
            if (elapsed > 65535) elapsed = 65535;
            history.ts_offsets[idx] = (uint16_t)elapsed;
            history.head = (idx + 1) % HIST_SIZE;
            if (history.count < HIST_SIZE) history.count++;
            xSemaphoreGive(dataMutex);
        }
        lastHistCapture = now; 
    }

    if (!running || finished) {
        fullPowerStartTime = 0;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if(myPID.GetMode() != MANUAL) myPID.SetMode(MANUAL);
            xSemaphoreGive(dataMutex);
        }
        return; 
    }

    if (!snapTimerActive) {
        if (heatStartTime == 0) heatStartTime = now;        
        if (snapInput >= (snapSP - 10.0)) {
            heatStartTime = now; 
        }

        if (now - heatStartTime > HEAT_TIMEOUT_MS) { emergencyStop(L_ERR_TIMEOUT); return; }
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if(myPID.GetMode() != AUTOMATIC) myPID.SetMode(AUTOMATIC);
        myPID.Compute();
        xSemaphoreGive(dataMutex);
    }
    
    if (Output >= PID_WINDOW_SIZE * 0.9) { 
        if (fullPowerStartTime == 0) { fullPowerStartTime = now; runawayBaselineTemp = snapInput; }
        else if (now - fullPowerStartTime > RUNAWAY_TIMEOUT_MS) {
            if (snapInput < (snapSP - 10)) {
                if ((snapInput - runawayBaselineTemp) < 5.0) { emergencyStop(L_ERR_RUNAWAY); return; } 
                else { runawayBaselineTemp = snapInput; fullPowerStartTime = now; }
            }
        }
    } else if (Output < PID_WINDOW_SIZE * 0.5) { fullPowerStartTime = 0; }

    // --- TIMER TRIGGER LOGIC (Uses TC2 if detected) ---
    double triggerTemp = snapHasTC2 ? snapInput2 : snapInput;
    double err = fabs(triggerTemp - snapSP);
    bool isStable = false;

    if (snapSP < 50.0 && snapStepStart > snapSP) {
        isStable = (triggerTemp <= snapSP + 5.0);
    } else if (snapSP > snapStepStart) {
        isStable = (triggerTemp >= snapSP - STABLE_TOLERANCE);
    } else {
        isStable = (err <= STABLE_TOLERANCE);
    }

    if (!snapTimerActive) {
        if (isStable) {
            if (stabStart == 0) { 
                stabStart = now; 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    strncpy(statusMsg, L_STATE_STABLE, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                    xSemaphoreGive(dataMutex);
                }
            }
            else if (now - stabStart >= STABLE_REQUIRED_TIME) {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    timerActive = true; 
                    snapTimerActive = true;
                    procStart = now; 
                    heatStartTime = 0;
                    currentStepPeak = snapInput; 
                    strncpy(statusMsg, L_STATE_HOLD, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                    xSemaphoreGive(dataMutex);
                }
            }
        } else {
            if (stabStart != 0 && (now - stabStart > 2000)) {
                stabStart = now - 2000; // Penalize the timer, but don't reset to 0 completely
            } else {
                stabStart = 0; 
        }
}
    } else {
        if ((now - procStart)/1000 >= snapHold*60) {
            if (profMode) {
                int nextStep = snapProfStep + 1;
                if (activeProfilePtr && nextStep < activeProfilePtr->numSteps) {
                    beginStep(nextStep);
                } else {
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        running = false; finished = true; profMode = false; 
                        portENTER_CRITICAL(&ssrmux);
                        digitalWrite(PIN_SSR, LOW); 
                        portEXIT_CRITICAL(&ssrmux);
                        strncpy(statusMsg, L_STATE_DONE, sizeof(statusMsg) - 1);
                        statusMsg[sizeof(statusMsg) - 1] = '\0';
                        forceHistoryCapture = true;
                        xSemaphoreGive(dataMutex);
                    }
                }
            } else {
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    running = false; finished = true; 
                    portENTER_CRITICAL(&ssrmux);
                    digitalWrite(PIN_SSR, LOW);
                    portEXIT_CRITICAL(&ssrmux);
                    strncpy(statusMsg, L_STATE_END, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                    xSemaphoreGive(dataMutex);
                }
            }
        }
    }
}