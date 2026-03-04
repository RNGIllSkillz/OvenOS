#pragma once
#include <PID_v1.h>
#include <max6675.h>
#include <ESPAsyncWebServer.h> 
#include "types.h"
#include "config.h"

// Hardware Objects
extern MAX6675 thermocouple;
extern MAX6675 thermocouple2;
extern PID myPID;
extern AsyncWebServer server; 
extern portMUX_TYPE ssrmux;
extern SemaphoreHandle_t dataMutex;

// PID Variables
extern double Setpoint, Input, Output;
extern uint32_t windowStartTime;

// Secondary Sensor & Fan State
extern double Input2;
extern volatile bool hasTC2;
extern volatile bool fanState;

// State Flags
extern volatile bool running;
extern volatile bool timerActive;
extern volatile bool finished;
extern bool profMode;
extern int profStep;
extern char statusMsg[64];
extern volatile bool emergencyStopped;
extern volatile bool monitoring;

// TC Verification State
extern volatile bool tcVerifyPending;

// Safety State
extern double currentStepPeak; 
extern double stepStartTemp;

// Profile State
extern const Profile* activeProfilePtr;
extern Profile customProfile;
extern char customName[32];
extern volatile bool hasCustomProfile;

// Timing & Safety State
extern uint32_t runStart;
extern uint32_t stabStart;
extern uint32_t procStart;
extern unsigned long holdMin;
extern volatile uint32_t ssrDeadmanKick;
extern uint32_t heatStartTime;
extern double lastValidTemp;
extern int tcFailCount;

// History State
extern HistoryBuffer history;
extern uint32_t lastHistCapture;
extern volatile bool forceHistoryCapture;

// Helper Prototypes
void emergencyStop(const char* reason);
void resetRunState();
void beginStep(int step);
extern bool tcFirstRead;