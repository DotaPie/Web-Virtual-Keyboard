#include "globals.h"

#include <WebServer.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#include "config.h"

USBHIDKeyboard Keyboard;
HardwareSerial LogSerial(UART_NUMBER);
WebServer server(SERVER_PORT);
Preferences prefs;