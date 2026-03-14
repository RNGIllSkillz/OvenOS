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
        digitalWrite(PIN_FAN, LOW); //LOW is on
        fanState = false;   
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
        CascadeDelta = 0;
        tcVerifyPending = false;
        myPID.SetMode(MANUAL);
        outerPID.SetMode(MANUAL);
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
        CascadeDelta = 0;
        tcFailCount = 0; 
        tcVerifyPending = false;
        myPID.SetMode(MANUAL);
        outerPID.SetMode(MANUAL);
        runawayBaselineTemp = 0;
        fullPowerStartTime = 0;
        strncpy(statusMsg, L_STATE_STOPPED, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';
        xSemaphoreGive(dataMutex);
    }
}

void beginStep(int step) {
    const Profile* p = nullptr;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        p = activeProfilePtr;
        xSemaphoreGive(dataMutex);
    }
    
    if (!p || step < 0 || step >= p->numSteps) {
        emergencyStop(L_ERR_INVALID_STEP);
        return;
    }
    const ProfileStep& s = p->steps[step];
    
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
        currentStepPeak2 = Input2;
        stepStartTemp2 = Input2;
        profStep = step;
        
        CascadeDelta = 0; // Reset windup delta
        
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
        CascadeDelta = 0;
        fullPowerStartTime = 0; 
        
        currentStepPeak = 0; 
        stepStartTemp = 0;
        currentStepPeak2 = 0;
        stepStartTemp2 = 0;
        
        runawayBaselineTemp = 0; 
        tcFirstRead = true;
        
        myPID.SetMode(MANUAL);
        myPID.SetOutputLimits(0, PID_WINDOW_SIZE);
        
        outerPID.SetMode(MANUAL);
        outerPID.SetOutputLimits(-CASCADE_MAX_UNDERSHOOT, CASCADE_MAX_OVERSHOOT);
        
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
    float cpuTemp = temperatureRead(); 
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        
        bool autoFan = fanState; 
        
        if (tcFailCount > 0 || isnan(Input) || Input <= 0.01) {
            autoFan = false;
        } 
        else if (Input > 50.0) {
            autoFan = false;
        } 
        else if (cpuTemp > 60.0) {
            autoFan = false;
        } 
        else if (cpuTemp < 58.0) {
            autoFan = true;
        }

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

    double snapSP, snapInput, snapInput2, snapStepStart, snapStepStart2, snapPeak, snapPeak2;
    unsigned long snapHold;
    bool snapTimerActive, snapHasTC2, snapCascade, snapRunning, snapFinished;
    int snapProfStep;
    
    // 1. Snapshot critical state safely
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        snapSP = Setpoint;
        snapHold = holdMin;
        snapInput = Input;
        snapInput2 = Input2;
        snapHasTC2 = hasTC2;
        snapCascade = cascadeMode;
        snapStepStart = stepStartTemp;
        snapStepStart2 = stepStartTemp2;
        snapPeak = currentStepPeak;
        snapPeak2 = currentStepPeak2;
        snapTimerActive = timerActive;
        snapProfStep = profStep;
        snapRunning = running;
        snapFinished = finished;
        xSemaphoreGive(dataMutex);
    } else { return; }
    
    // Global Safety Max Temp Checks
    if (snapInput > SAFETY_MAX_TEMP) { emergencyStop(L_ERR_MAX_TEMP); return; }
    if (snapHasTC2 && snapInput2 > SAFETY_MAX_TEMP) { emergencyStop(L_ERR_MAX_TEMP); return; }
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

    if (snapRunning) {
        
        // Air Overshoot Check
        double maxExpected = (snapSP > snapStepStart) ? snapSP : snapStepStart;
        if (snapCascade && snapHasTC2) maxExpected += CASCADE_MAX_OVERSHOOT; 
        
        if (snapInput > (maxExpected + 20.0) && snapInput > 60.0) {
            emergencyStop(L_ERR_OVERSHOOT); return;
        }
        
        // Part Overshoot Check
        if (snapCascade && snapHasTC2) {
            double maxExpectedPart = (snapSP > snapStepStart2) ? snapSP : snapStepStart2;
            if (snapInput2 > (maxExpectedPart + 20.0) && snapInput2 > 60.0) {
                emergencyStop(L_ERR_OVERSHOOT); return;
            }
        }
        
        // Peak Tracking (Air)
        if (snapInput > snapPeak) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                currentStepPeak = snapInput; snapPeak = snapInput;
                xSemaphoreGive(dataMutex);
            }
        }
        // Peak Tracking (Part)
        if (snapHasTC2 && snapInput2 > snapPeak2) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                currentStepPeak2 = snapInput2; snapPeak2 = snapInput2;
                xSemaphoreGive(dataMutex);
            }
        }

        // Air Temp Drop Check
        double checkTarget = snapSP;
        if (snapCascade && snapHasTC2) {
            double tempDelta = 0;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                tempDelta = CascadeDelta;
                xSemaphoreGive(dataMutex);
            }
            checkTarget = snapSP + tempDelta;
            if (checkTarget > SAFETY_MAX_TEMP) checkTarget = SAFETY_MAX_TEMP;
            if (checkTarget < 0) checkTarget = 0;
        }

        if (checkTarget >= snapStepStart) {
            double effectivePeak = (snapPeak > checkTarget) ? checkTarget : snapPeak;
            if (snapInput < checkTarget && snapPeak > 40 && snapInput < (effectivePeak - 15.0)) {
                emergencyStop(L_ERR_DROP); return;
            }
        }

        // Part Temp Drop Check
        if (snapCascade && snapHasTC2 && snapSP >= snapStepStart2) {
            double effectivePeak2 = (snapPeak2 > snapSP) ? snapSP : snapPeak2;
            if (snapInput2 < snapSP && snapPeak2 > 40 && snapInput2 < (effectivePeak2 - 15.0)) {
                emergencyStop(L_ERR_DROP); return;
            }
        }
    }

    // 2. History Capture
    bool shouldCapture = false;
    if (forceHistoryCapture) {
        shouldCapture = true; forceHistoryCapture = false;
    } else if ((snapRunning || monitoring || snapInput > 25.0) && (now - lastHistCapture >= HIST_INTERVAL)) {
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

    if (!snapRunning || snapFinished) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            fullPowerStartTime = 0;
            if(myPID.GetMode() != MANUAL) myPID.SetMode(MANUAL);
            if(outerPID.GetMode() != MANUAL) outerPID.SetMode(MANUAL);
            xSemaphoreGive(dataMutex);
        }
        return; 
    }

    if (!snapTimerActive) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (heatStartTime == 0) heatStartTime = now;        
            if (Input >= (Setpoint - 10.0)) {
                heatStartTime = now; 
            }
            if (now - heatStartTime > HEAT_TIMEOUT_MS) { 
                xSemaphoreGive(dataMutex);
                emergencyStop(L_ERR_TIMEOUT); return; 
            }
            xSemaphoreGive(dataMutex);
        }
    }

    // 3. FULL CASCADE CONTROL INJECTION
    double snapOutput = 0;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (cascadeMode && hasTC2 && running) {
            if (outerPID.GetMode() != AUTOMATIC) outerPID.SetMode(AUTOMATIC);
            
            // Calculate delta offset required to bring Sand/Part to target
            outerPID.Compute(); 
            
            double dynamicSP = Setpoint + CascadeDelta;
            if (dynamicSP > SAFETY_MAX_TEMP) dynamicSP = SAFETY_MAX_TEMP;
            if (dynamicSP < 0) dynamicSP = 0;
            
            // Temporarily apply dynamic SP to Inner Loop
            double origSP = Setpoint;
            Setpoint = dynamicSP;
            
            if (myPID.GetMode() != AUTOMATIC) myPID.SetMode(AUTOMATIC);
            myPID.Compute();
            
            Setpoint = origSP; // Restore so UI tracking and logic aren't mangled
        } else {
            if (outerPID.GetMode() != MANUAL) outerPID.SetMode(MANUAL);
            if (myPID.GetMode() != AUTOMATIC) myPID.SetMode(AUTOMATIC);
            myPID.Compute();
        }
        snapOutput = Output;
        xSemaphoreGive(dataMutex);
    }
    
    // 4. Runaway checks using snapshotted output
    if (snapOutput >= PID_WINDOW_SIZE * 0.9) { 
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (fullPowerStartTime == 0) { fullPowerStartTime = now; runawayBaselineTemp = Input; }
            else if (now - fullPowerStartTime > RUNAWAY_TIMEOUT_MS) {
                
                // Compare to the ACTUAL target for the heater (Air target, even if cascade is shifting it)
                double activeTarget = Setpoint;
                if (cascadeMode && hasTC2) {
                    activeTarget += CascadeDelta;
                    if (activeTarget > SAFETY_MAX_TEMP) activeTarget = SAFETY_MAX_TEMP;
                }
                
                if (Input < (activeTarget - 10.0)) {
                    if ((Input - runawayBaselineTemp) < 5.0) { 
                        xSemaphoreGive(dataMutex);
                        emergencyStop(L_ERR_RUNAWAY); return; 
                    } 
                    else { runawayBaselineTemp = Input; fullPowerStartTime = now; }
                }
            }
            xSemaphoreGive(dataMutex);
        }
    } else if (snapOutput < PID_WINDOW_SIZE * 0.5) { 
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            fullPowerStartTime = 0; 
            xSemaphoreGive(dataMutex);
        }
    }

    // 5. TIMER TRIGGER LOGIC
    double triggerTemp = (snapCascade && snapHasTC2) ? snapInput2 : snapInput;
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
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (stabStart == 0) { 
                    stabStart = now; 
                    strncpy(statusMsg, L_STATE_STABLE, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                }
                else if (now - stabStart >= STABLE_REQUIRED_TIME) {
                    timerActive = true; 
                    snapTimerActive = true;
                    procStart = now; 
                    heatStartTime = 0;
                    currentStepPeak = Input; 
                    strncpy(statusMsg, L_STATE_HOLD, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                }
                xSemaphoreGive(dataMutex);
            }
        } else {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (stabStart != 0 && (now - stabStart > 2000)) {
                    stabStart = now - 2000; // Penalize, but don't reset completely
                } else {
                    stabStart = 0; 
                }
                xSemaphoreGive(dataMutex);
            }
        }
    } else {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if ((now - procStart)/1000 >= holdMin*60) {
                if (profMode) {
                    int nextStep = profStep + 1;
                    if (activeProfilePtr && nextStep < activeProfilePtr->numSteps) {
                        xSemaphoreGive(dataMutex); // Must release before beginStep acquires it again!
                        beginStep(nextStep);
                        return;
                    } else {
                        running = false; finished = true; profMode = false; 
                        portENTER_CRITICAL(&ssrmux);
                        digitalWrite(PIN_SSR, LOW); 
                        portEXIT_CRITICAL(&ssrmux);
                        strncpy(statusMsg, L_STATE_DONE, sizeof(statusMsg) - 1);
                        statusMsg[sizeof(statusMsg) - 1] = '\0';
                        forceHistoryCapture = true;
                    }
                } else {
                    running = false; finished = true; 
                    portENTER_CRITICAL(&ssrmux);
                    digitalWrite(PIN_SSR, LOW);
                    portEXIT_CRITICAL(&ssrmux);
                    strncpy(statusMsg, L_STATE_END, sizeof(statusMsg) - 1);
                    statusMsg[sizeof(statusMsg) - 1] = '\0';
                    forceHistoryCapture = true;
                }
            }
            xSemaphoreGive(dataMutex);
        }
    }
}