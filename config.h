#pragma once
#include "credentials.h"

// PIN DEFINITIONS
const int PIN_SSR     = 4;
const int PIN_TC_DO   = 19;
const int PIN_TC_CS   = 5;
const int PIN_TC_CLK  = 18;

// PID TUNING 
double PID_KP = 80.0;
double PID_KI = 0.4;
double PID_KD = 1.5;
const int PID_WINDOW_SIZE = 1000;

// SAFETY SETTINGS
const double        STABLE_TOLERANCE     = 2.0;    // Deg C
const unsigned long STABLE_REQUIRED_TIME = 10000;  // ms
const double        SAFETY_MAX_TEMP      = 300.0;  // Deg C 
const int           WDT_TIMEOUT_SEC      = 5;      // Seconds
const unsigned long SSR_DEADMAN_MS       = 2000;   // Safety cutoff
const unsigned long HEAT_TIMEOUT_MS      = 60UL * 60UL * 1000UL; // 1hr max
const double        MAX_RISE_PER_READ    = 10.0;
const int           TC_FAIL_LIMIT        = 4;      

// HISTORY CONFIGURATION
const unsigned long HIST_INTERVAL        = 60000;  // 1 minute per point
const int           HIST_SIZE            = 600;    // 10 hours of total history retention