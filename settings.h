#pragma once
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "state.h"
#include "config.h"

void saveSettings() {
    File f = LittleFS.open("/settings.json", "w");
    if (!f) return;
    
    StaticJsonDocument<512> doc; // Expanded
    doc["kp"] = PID_KP;
    doc["ki"] = PID_KI;
    doc["kd"] = PID_KD;
    doc["kpO"] = PID_OUTER_KP;
    doc["kiO"] = PID_OUTER_KI;
    doc["kdO"] = PID_OUTER_KD;
    doc["casc"] = cascadeMode;
    
    serializeJson(doc, f);
    f.close();
    Serial.println("[SETTINGS] Saved");
}

void loadSettings() {
    if (!LittleFS.exists("/settings.json")) return;
    
    File f = LittleFS.open("/settings.json", "r");
    if (!f) return;
    
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    
    if (!err) {
        if (doc.containsKey("kp")) PID_KP = doc["kp"];
        if (doc.containsKey("ki")) PID_KI = doc["ki"];
        if (doc.containsKey("kd")) PID_KD = doc["kd"];
        
        if (doc.containsKey("kpO")) PID_OUTER_KP = doc["kpO"];
        if (doc.containsKey("kiO")) PID_OUTER_KI = doc["kiO"];
        if (doc.containsKey("kdO")) PID_OUTER_KD = doc["kdO"];
        if (doc.containsKey("casc")) cascadeMode = doc["casc"];
        
        myPID.SetTunings(PID_KP, PID_KI, PID_KD);
        outerPID.SetTunings(PID_OUTER_KP, PID_OUTER_KI, PID_OUTER_KD);
        Serial.printf("[SETTINGS] Loaded\n");
    }
}