#pragma once
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "state.h"
#include "config.h"

void saveSettings() {
    File f = LittleFS.open("/settings.json", "w");
    if (!f) return;
    
    StaticJsonDocument<256> doc;
    doc["kp"] = PID_KP;
    doc["ki"] = PID_KI;
    doc["kd"] = PID_KD;
    
    serializeJson(doc, f);
    f.close();
    Serial.println("[SETTINGS] PID Saved");
}

void loadSettings() {
    if (!LittleFS.exists("/settings.json")) return;
    
    File f = LittleFS.open("/settings.json", "r");
    if (!f) return;
    
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    
    if (!err) {
        if (doc.containsKey("kp")) PID_KP = doc["kp"];
        if (doc.containsKey("ki")) PID_KI = doc["ki"];
        if (doc.containsKey("kd")) PID_KD = doc["kd"];
        
        // Update the live PID object
        myPID.SetTunings(PID_KP, PID_KI, PID_KD);
        Serial.printf("[SETTINGS] Loaded PID: P=%.1f I=%.2f D=%.2f\n", PID_KP, PID_KI, PID_KD);
    }
}