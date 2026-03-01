#pragma once
#include "language.h"
#include "state.h"
#include "config.h"

// TC Verification State
uint32_t tcVerifyStartTime = 0;
double tcVerifyFirstReading = 0;

// Thermal Runaway Protection
uint32_t fullPowerStartTime = 0;
const uint32_t RUNAWAY_TIMEOUT_MS = 180000; 

// Runaway detection baseline
double runawayBaselineTemp = 0;

void onDeadmanTimer(void* arg) {
    if ((millis() - ssrDeadmanKick) > SSR_DEADMAN_MS) {
        portENTER_CRITICAL(&ssrmux);
        digitalWrite(PIN_SSR, LOW);
        emergencyStopped = true; 
        portEXIT_CRITICAL(&ssrmux);
    }
}

void emergencyStop(const char* reason) {
    portENTER_CRITICAL(&ssrmux);
    digitalWrite(PIN_SSR, LOW);
    emergencyStopped = true;
    portEXIT_CRITICAL(&ssrmux);
    
    running = false;
    timerActive = false;
    finished = false;
    profMode = false;
    activeProfilePtr = nullptr;
    profStep = -1;
    Output = 0;
    
    tcVerifyPending = false;
    myPID.SetMode(MANUAL);
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snprintf(statusMsg, sizeof(statusMsg), "%s%s", L_ERR_PREFIX, reason);
        xSemaphoreGive(dataMutex);
    }
    Serial.printf("!!! EMERGENCY STOP: %s !!!\n", reason);
}

void stopReflow() {
    digitalWrite(PIN_SSR, LOW);
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
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(statusMsg, L_STATE_STOPPED, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        xSemaphoreGive(dataMutex);
    }
    Serial.println("[INFO] User stopped run.");
}

void beginStep(int step) {
    if (!activeProfilePtr || step < 0 || step >= activeProfilePtr->numSteps) {
        emergencyStop(L_ERR_INVALID_STEP);
        return;
    }
    const ProfileStep& s = activeProfilePtr->steps[step];
    
    // Lock the 64-bit writes to prevent torn reads
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Setpoint = s.targetTemp;
        holdMin = s.holdMin;
        strncpy(statusMsg, s.label, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        xSemaphoreGive(dataMutex);
    } else {
        // Fallback if heavily congested
        Setpoint = s.targetTemp;
        holdMin = s.holdMin;
    }

    timerActive = false;
    stabStart = 0;
    procStart = 0;
    heatStartTime = 0;
    finished = false;
    currentStepPeak = Input; 
    stepStartTemp = Input; 
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(statusMsg, s.label, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        xSemaphoreGive(dataMutex);
    }
}

void resetRunState() {    
    running = false;
    monitoring = false; 
    timerActive = false;
    finished = false;
    profStep = -1;
    activeProfilePtr = nullptr;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        history.head = 0;
        history.count = 0;
        xSemaphoreGive(dataMutex);
    }
    
    runStart = millis();
    lastHistCapture = runStart;
    heatStartTime = 0;
    tcFailCount = 0;
    tcVerifyPending = false;
    stabStart = 0;
    Output = 0;
    fullPowerStartTime = 0; 
    
    currentStepPeak = 0; 
    stepStartTemp = 0;
    runawayBaselineTemp = 0; 
    
    digitalWrite(PIN_SSR, LOW);
    
    myPID.SetMode(MANUAL);
    myPID.SetOutputLimits(0, PID_WINDOW_SIZE);
    windowStartTime = millis();
    tcFirstRead = true;
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
        if (now - tcVerifyStartTime < 20) return;
        
        double v2 = thermocouple.readCelsius();
        tcVerifyPending = false;
        
        if (!isValidTCReading(v2)) {
             tcFailCount++;
             return;
        }
        
        if (fabs(v2 - tcVerifyFirstReading) < 10.0) {
             Input = v2;
             lastValidTemp = v2;
             tcFailCount = 0;
        }
        return;
    }

    double v = thermocouple.readCelsius();

    if (!isValidTCReading(v)) {
        tcFailCount++;
        if (tcFailCount >= TC_FAIL_LIMIT) {
            if (running) emergencyStop(L_ERR_TC_FAIL);
            else {
                 Input = 0; 
                 tcFailCount = 0; 
            }
        }
        return;
    }

    if (tcFirstRead) {
        Input = v;
        lastValidTemp = v;
        tcFirstRead = false;
        tcFailCount = 0;
        return;
    }

    if (lastValidTemp > 0.01 && fabs(v - lastValidTemp) > MAX_RISE_PER_READ) {
        tcVerifyPending = true;
        tcVerifyStartTime = now;
        tcVerifyFirstReading = v;
        return;
    }

    Input = (Input * 0.7) + (v * 0.3);
    lastValidTemp = v;
    tcFailCount = 0;
}

void runControlLoop() {
    uint32_t now = millis();

    double snapSP = Setpoint;
    unsigned long snapHold = holdMin;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        snapSP = Setpoint;
        snapHold = holdMin;
        xSemaphoreGive(dataMutex);
    }
    
    if (Input > SAFETY_MAX_TEMP) { 
        emergencyStop(L_ERR_MAX_TEMP); 
        return; 
    }

    if (tcVerifyPending) {
        return; 
    }

    bool estopFlag = false;
    portENTER_CRITICAL(&ssrmux);
    estopFlag = emergencyStopped;
    portEXIT_CRITICAL(&ssrmux);

    if (estopFlag) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
             // Checking exact prefix using strlen safely independent of actual language
             if (strncmp(statusMsg, L_ERR_PREFIX, strlen(L_ERR_PREFIX)) != 0) {
                 snprintf(statusMsg, sizeof(statusMsg), "%s%s", L_ERR_PREFIX, L_ERR_FAULT);
                 forceHistoryCapture = true;
             }
             xSemaphoreGive(dataMutex);
        }
        return; 
    }

    if (running) {
        double maxExpected = (snapSP > stepStartTemp) ? snapSP : stepStartTemp;
        
        if (Input > (maxExpected + 20.0) && maxExpected > 40) {
            emergencyStop(L_ERR_OVERSHOOT);
            return;
        }
    
        if (!timerActive) {
            if (snapSP >= stepStartTemp) {
                if (Input > currentStepPeak) currentStepPeak = Input;
                
                if (Input < snapSP && currentStepPeak > 40 && Input < (currentStepPeak - 15.0)) {
                    emergencyStop(L_ERR_DROP); 
                    return;
                }
            }
        }
    }

    bool shouldCapture = false;
    if (forceHistoryCapture) {
        shouldCapture = true;
        forceHistoryCapture = false;
    } else if ((running || monitoring || Input > 25.0) && (now - lastHistCapture >= HIST_INTERVAL)) {
        shouldCapture = true;
    }
    if (shouldCapture) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            int idx = history.head;
            history.temps[idx] = (int16_t)round(Input * 10.0);
            history.sps[idx] = (int16_t)round(snapSP);
            
            // Calculate relative seconds. Max out at 65535 (18.2 hours) to fit in uint16_t
            uint32_t elapsed = (now >= runStart) ? (now - runStart) / 1000 : 0;
            if (elapsed > 65535) elapsed = 65535;
            history.ts_offsets[idx] = (uint16_t)elapsed;
            
            history.head = (idx + 1) % HIST_SIZE;
            if (history.count < HIST_SIZE) history.count++;
            xSemaphoreGive(dataMutex);
        }
        lastHistCapture = now; // Reset timer so the next interval starts counting from NOW
    }

    if (!running || finished) {
        fullPowerStartTime = 0;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if(myPID.GetMode() != MANUAL) myPID.SetMode(MANUAL);
            xSemaphoreGive(dataMutex);
        }
        return; 
    }

    if (!timerActive) {
        if (heatStartTime == 0) heatStartTime = now;
        if (now - heatStartTime > HEAT_TIMEOUT_MS) { emergencyStop(L_ERR_TIMEOUT); return; }
    }

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if(myPID.GetMode() != AUTOMATIC) myPID.SetMode(AUTOMATIC);
        myPID.Compute();
        xSemaphoreGive(dataMutex);
    }
    
    if (Output >= PID_WINDOW_SIZE * 0.9) { 
        if (fullPowerStartTime == 0) {
            fullPowerStartTime = now;
            runawayBaselineTemp = Input;
        }
        else if (now - fullPowerStartTime > RUNAWAY_TIMEOUT_MS) {
            if (Input < (snapSP - 10)) {
                if ((Input - runawayBaselineTemp) < 5.0) {
                     emergencyStop(L_ERR_RUNAWAY);
                     return;
                } else {
                     runawayBaselineTemp = Input;
                     fullPowerStartTime = now; 
                }
            }
        }
    } else if (Output < PID_WINDOW_SIZE * 0.85) {
        fullPowerStartTime = 0;
    }

    double err = fabs(Input - snapSP);
    bool isStable = false;

    if (snapSP < 50.0 && stepStartTemp > snapSP) {
        isStable = (Input <= snapSP + 5.0);
    } else {
        isStable = (err <= STABLE_TOLERANCE);
    }

    if (!timerActive) {
        if (isStable) {
            if (stabStart == 0) { 
                stabStart = now; 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    strncpy(statusMsg, L_STATE_STABLE, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                    xSemaphoreGive(dataMutex);
                }
            }
            else if (now - stabStart >= STABLE_REQUIRED_TIME) {
                timerActive = true; 
                procStart = now; 
                heatStartTime = 0;
                currentStepPeak = Input; 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    strncpy(statusMsg, L_STATE_HOLD, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                    xSemaphoreGive(dataMutex);
                }
            }
        } else {
            stabStart = 0;
        }
    } else {
        if ((now - procStart)/1000 >= snapHold*60) {
            if (profMode) {
                if (++profStep < activeProfilePtr->numSteps) beginStep(profStep);
                else {
                    running = false; finished = true; profMode = false; 
                    digitalWrite(PIN_SSR, LOW); 
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        strncpy(statusMsg, L_STATE_DONE, sizeof(statusMsg) - 1);
                        statusMsg[sizeof(statusMsg) - 1] = '\0';
                        forceHistoryCapture = true;
                        xSemaphoreGive(dataMutex);
                    }
                }
            } else {
                running = false; finished = true; 
                digitalWrite(PIN_SSR, LOW); 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    strncpy(statusMsg, L_STATE_END, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                    xSemaphoreGive(dataMutex);
                }
            }
        }
    }
}