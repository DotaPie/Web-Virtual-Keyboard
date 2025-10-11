#ifndef HTTPHANDLERS_H
#define HTTPHANDLERS_H

#include <ArduinoJson.h>

void handleRoot();
void handleType();
void handleGetPresets();
void handlePostPreset();
void handleDeletePreset();

#endif