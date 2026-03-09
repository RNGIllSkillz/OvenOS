#pragma once
#include "credentials.h"

// PIN DEFINITIONS

//solid state relay
const int PIN_SSR     = 17;

//MAX6675_1
const int PIN_TC_CLK  = 15;
const int PIN_TC_CS   = 2;
const int PIN_TC_DO   = 4;

//MAX6675_2
const int PIN_TC2_CS  = 16;

//FAN
const int PIN_FAN     = 27;

// PID TUNING
extern double PID_KP;
extern double PID_KI;
extern double PID_KD;
const int PID_WINDOW_SIZE = 250;

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
const unsigned long HIST_INTERVAL        = 30000;  // 30 seconds per point
const int           HIST_SIZE            = 1200;    // 10 hours of total history retention