#ifndef HTTPHANDLERS_H
#define HTTPHANDLERS_H

#include <Arduino.h>
#include <ArduinoJson.h>

void sendValue(const String& v);

void handleRoot();
void handleType();
void handleGetInfo();
void handleGetPresets();
void handlePostPreset();
void handleDeletePreset();
void handleSendPreset();

// MicroSD REST handlers
void handleSdStatus();
void handleSdList();
void handleSdFile();
void handleSdUpload();
void handleSdUploadChunk();
void handleSdSave();
void handleSdDelete();
void handleSdRun();

#endif