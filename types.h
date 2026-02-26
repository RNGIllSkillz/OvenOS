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
  float temps[HIST_SIZE];
  float sps[HIST_SIZE];
  uint32_t timestamps[HIST_SIZE];
  volatile int head;
  volatile int count;
};