#ifndef GLOBALS_H
#define GLOBALS_H

#include <WebServer.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

extern USBHIDKeyboard Keyboard;
extern HardwareSerial LogSerial;
extern WebServer server;
extern Preferences prefs;

#endif