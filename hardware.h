#pragma once
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

void IRAM_ATTR onDeadmanTimer(void* arg) {
    if ((millis() - ssrDeadmanKick) > SSR_DEADMAN_MS) {
        portENTER_CRITICAL_ISR(&ssrmux); 
        digitalWrite(PIN_SSR, LOW);
        emergencyStopped = true; 
        portEXIT_CRITICAL_ISR(&ssrmux);
    }
}

void emergencyStop(const char* reason) {
    // NOTE: emergencyStop must NEVER be called from inside another portENTER_CRITICAL(&ssrmux) block to avoid deadlock.
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
        snprintf(statusMsg, sizeof(statusMsg), "АВАРИЯ: %s", reason);
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
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(statusMsg, "ОСТАНОВЛЕНО", sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        xSemaphoreGive(dataMutex);
    }
    Serial.println("[INFO] User stopped run.");
}

void beginStep(int step) {
    if (!activeProfilePtr || step < 0 || step >= activeProfilePtr->numSteps) {
        emergencyStop("НЕВЕРНЫЙ ШАГ");
        return;
    }
    const ProfileStep& s = activeProfilePtr->steps[step];
    Setpoint = s.targetTemp;
    holdMin = s.holdMin;
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
    
    // NEVER clear emergencyStopped here. It should only be cleared by a manual API reset action.
    digitalWrite(PIN_SSR, LOW);
    
    myPID.SetMode(MANUAL);
    myPID.SetOutputLimits(0, PID_WINDOW_SIZE);
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
            if (running) emergencyStop("ТЕРМОПАРА");
            else {
                 Input = 0; 
                 tcFailCount = 0; 
            }
        }
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
    
    if (Input > SAFETY_MAX_TEMP) { emergencyStop("MAX TEMP LIMIT"); return; }

    bool estopFlag = false;
    portENTER_CRITICAL(&ssrmux);
    estopFlag = emergencyStopped;
    portEXIT_CRITICAL(&ssrmux);

    if (estopFlag) { 
        digitalWrite(PIN_SSR, LOW); 
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
             // UTF-8: "АВАРИЯ" = 12 bytes
             if (strncmp(statusMsg, "АВАРИЯ", 12) != 0) strncpy(statusMsg, "АВАРИЯ: СБОЙ", 63);
             xSemaphoreGive(dataMutex);
        }
        return; 
    }

    if (running) {
        double maxExpected = (Setpoint > stepStartTemp) ? Setpoint : stepStartTemp;
        
        if (Input > (maxExpected + 20.0) && maxExpected > 40) {
            emergencyStop("ПЕРЕГРЕВ (>SP+20)");
            return;
        }

        if (!timerActive) {
            if (Setpoint >= stepStartTemp) {
                if (Input > currentStepPeak) currentStepPeak = Input;
                
                if (Input < Setpoint && currentStepPeak > 40 && Input < (currentStepPeak - 15.0)) {
                    emergencyStop("ПАДЕНИЕ ТЕМП."); 
                    return;
                }
            }
        }
    }

    if ((running || monitoring || Input > 25.0) && (now - lastHistCapture >= HIST_INTERVAL)) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            int idx = history.head;
            history.temps[idx] = Input;
            history.sps[idx] = Setpoint;
            history.timestamps[idx] = (now - runStart)/1000;
            history.head = (idx + 1) % HIST_SIZE;
            if (history.count < HIST_SIZE) history.count++;
            xSemaphoreGive(dataMutex);
        }
        lastHistCapture = now;
    }

    if (!running || finished || tcVerifyPending) { 
        digitalWrite(PIN_SSR, LOW); 
        fullPowerStartTime = 0;
        if(myPID.GetMode() != MANUAL) myPID.SetMode(MANUAL);
        return; 
    }

    if (!timerActive) {
        if (heatStartTime == 0) heatStartTime = now;
        if (now - heatStartTime > HEAT_TIMEOUT_MS) { emergencyStop("ТАЙМАУТ"); return; }
    }

    if(myPID.GetMode() != AUTOMATIC) myPID.SetMode(AUTOMATIC);
    myPID.Compute();
    
    uint32_t wElapsed = now - windowStartTime;
    while (wElapsed >= (uint32_t)PID_WINDOW_SIZE) { 
        windowStartTime += PID_WINDOW_SIZE; 
        wElapsed -= PID_WINDOW_SIZE; 
    }
    
    if (tcFailCount == 0) digitalWrite(PIN_SSR, (Output > wElapsed));
    else digitalWrite(PIN_SSR, LOW);

    if (Output >= PID_WINDOW_SIZE * 0.9) { 
        if (fullPowerStartTime == 0) {
            fullPowerStartTime = now;
            runawayBaselineTemp = Input;
        }
        else if (now - fullPowerStartTime > RUNAWAY_TIMEOUT_MS) {
            if (Input < (Setpoint - 10)) {
                if ((Input - runawayBaselineTemp) < 5.0) {
                     emergencyStop("ТЕПЛОВОЙ РАЗГОН");
                     return;
                } else {
                     runawayBaselineTemp = Input;
                     fullPowerStartTime = now; 
                }
            }
        }
    } else if (Output < PID_WINDOW_SIZE * 0.5) {
        fullPowerStartTime = 0;
    }

    double err = fabs(Input - Setpoint);
    if (!timerActive) {
        if (err <= STABLE_TOLERANCE) {
            if (stabStart == 0) { 
                stabStart = now; 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    strncpy(statusMsg, "СТАБИЛИЗАЦИЯ", 63); 
                    xSemaphoreGive(dataMutex);
                }
            }
            else if (now - stabStart >= STABLE_REQUIRED_TIME) {
                timerActive = true; 
                procStart = now; 
                heatStartTime = 0;
                currentStepPeak = Input; 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    strncpy(statusMsg, "ПОДДЕРЖКА", 63);
                    xSemaphoreGive(dataMutex);
                }
            }
        } else {
            stabStart = 0;
        }
    } else {
        if ((now - procStart)/1000 >= holdMin*60) {
            if (profMode) {
                if (++profStep < activeProfilePtr->numSteps) beginStep(profStep);
                else {
                    running = false; finished = true; profMode = false; 
                    digitalWrite(PIN_SSR, LOW); 
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        strncpy(statusMsg, "ГОТОВО", 63);
                        xSemaphoreGive(dataMutex);
                    }
                }
            } else {
                running = false; finished = true; 
                digitalWrite(PIN_SSR, LOW); 
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    strncpy(statusMsg, "КОНЕЦ", 63);
                    xSemaphoreGive(dataMutex);
                }
            }
        }
    }
}