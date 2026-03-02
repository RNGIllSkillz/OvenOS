#pragma once
#include "types.h"
#include "state.h"
#include "language.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

const Profile BUILTIN_PROFILES[] = {
  { L_PROF_SOLDER, 4, { {150,2, L_STEP_PREHEAT}, {180,2, L_STEP_SOAK}, {245,1, L_STEP_REFLOW}, {30,5, L_STEP_COOL} } },
  { L_PROF_ANNEAL, 2, { {170,30, L_STEP_ANNEAL}, {30,15, L_STEP_COOL} } },
  { L_PROF_PLA,    2, { {50,360, L_STEP_DRY}, {30,30, L_STEP_COOL} } },
  { L_PROF_PETG,   2, { {65,360, L_STEP_DRY}, {30,30, L_STEP_COOL} } },
  { L_PROF_ABS,    2, { {80,360, L_STEP_DRY}, {30,30, L_STEP_COOL} } },
  { L_PROF_PA,     3, { {70,120, L_STEP_PREDRY}, {85,360, L_STEP_DRY}, {30,60, L_STEP_COOL} } },
  { L_PROF_PC,     2, { {85,360, L_STEP_DRY}, {30,45, L_STEP_COOL} } },
  { L_PROF_PP,     2, { {60,240, L_STEP_DRY}, {30,30, L_STEP_COOL} } },
  { L_PROF_PPS,    2, { {130,240, L_STEP_DRY}, {30,60, L_STEP_COOL} } },
  { L_PROF_PEEK,   2, { {150,240, L_STEP_DRY}, {30,60, L_STEP_COOL} } },
};
const int NUM_BUILTIN = sizeof(BUILTIN_PROFILES) / sizeof(BUILTIN_PROFILES[0]);

void sanitizeStr(char* dst, const char* src, size_t maxLen) {
  if (!src) { dst[0] = '\0'; return; }
  size_t j = 0;
  for (size_t i = 0; src[i] && j < maxLen - 1; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c >= 0x20 && c != '"' && c != '\\') dst[j++] = src[i];
  }
  dst[j] = '\0';
}

void saveCustomProfile() {
  File f = LittleFS.open("/profile.json", "w");
  if(f) {
    StaticJsonDocument<1024> doc;
    doc["name"] = customName;
    JsonArray steps = doc.createNestedArray("steps");
    for(int i=0; i<customProfile.numSteps; i++) {
      JsonObject s = steps.createNestedObject();
      s["label"] = customProfile.steps[i].label;
      s["temp"] = customProfile.steps[i].targetTemp;
      s["hold"] = customProfile.steps[i].holdMin;
    }
    serializeJson(doc, f);
    f.close();
  }
}

void loadCustomProfile() {
  if (!LittleFS.exists("/profile.json")) return;
  File f = LittleFS.open("/profile.json", "r");
  if(!f) return;
  
  DynamicJsonDocument doc(2048);
  if(!deserializeJson(doc, f)) {
     sanitizeStr(customName, doc["name"] | L_DEFAULT_CUSTOM, sizeof(customName));
     customProfile.name = customName;
     JsonArray steps = doc["steps"];
     int c=0;
     for(JsonObject s : steps) {
       if(c>=8) break;
       // Ensuring strict type alignment
       customProfile.steps[c].targetTemp = s["temp"].as<double>();
       customProfile.steps[c].holdMin = s["hold"].as<unsigned long>();
       sanitizeStr(customProfile.steps[c].label, s["label"] | L_DEFAULT_STEP, 23);
       c++;
     }
     customProfile.numSteps = c;
     hasCustomProfile = (c>0);
  }
  f.close();
}