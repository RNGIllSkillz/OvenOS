#pragma once
#include <Arduino.h>
#include "config.h"

struct ProfileStep { 
  double targetTemp;
  unsigned long holdMin;
  char label[24];
};

struct Profile {
  const char* name;
  int numSteps;
  ProfileStep steps[8];
};

struct HistoryBuffer {
  int16_t temps[HIST_SIZE];
  int16_t temps2[HIST_SIZE];
  int16_t sps[HIST_SIZE];
  uint16_t ts_offsets[HIST_SIZE];
  volatile int head;
  volatile int count;
};