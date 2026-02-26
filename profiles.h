#pragma once
#include "types.h"
#include "state.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

const Profile BUILTIN_PROFILES[] = {
  { "Пайка оплавлением", 4, { {150,2, "Преднагрев"}, {180,2, "Выдержка"}, {245,1, "Оплавление"}, {30,5, "Охлаждение"} } },
  { "Отжиг ABS", 2, { {170,30, "Отжиг"}, {30,15, "Охлаждение"} } },
  { "PLA / TPU / PVA", 2, { {50,360, "Сушка"}, {30,30, "Охлаждение"} } },
  { "PETG / PET +CF/GF", 2, { {65,360, "Сушка"}, {30,30, "Охлаждение"} } },
  { "ABS / ASA +CF/GF", 2, { {80,360, "Сушка"}, {30,30, "Охлаждение"} } },
  { "PA / Nylon +CF/GF", 3, { {70,120, "Пред. сушка"}, {85,360, "Сушка"}, {30,60, "Охлаждение"} } },
  { "PC / PC-ABS", 2, { {85,360, "Сушка"}, {30,45, "Охлаждение"} } },
  { "PP +CF/GF", 2, { {60,240, "Сушка"}, {30,30, "Охлаждение"} } },
  { "PPS +CF/GF", 2, { {130,240, "Сушка"}, {30,60, "Охлаждение"} } },
  { "PEEK +CF/GF", 2, { {150,240, "Сушка"}, {30,60, "Охлаждение"} } },
};
const int NUM_BUILTIN = sizeof(BUILTIN_PROFILES) / sizeof(BUILTIN_PROFILES[0]);

void sanitizeStr(char* dst, const char* src, size_t maxLen) {
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
     sanitizeStr(customName, doc["name"]|"Custom", sizeof(customName));
     customProfile.name = customName;
     JsonArray steps = doc["steps"];
     int c=0;
     for(JsonObject s : steps) {
       if(c>=8) break;
       customProfile.steps[c].targetTemp = s["temp"];
       customProfile.steps[c].holdMin = s["hold"];
       sanitizeStr(customProfile.steps[c].label, s["label"], 23);
       c++;
     }
     customProfile.numSteps = c;
     hasCustomProfile = (c>0);
  }
  f.close();
}