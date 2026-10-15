#ifndef CONFIG_H_WRAPPER
#define CONFIG_H_WRAPPER

// If a private local configuration exists (ignored by git), include it:
#if __has_include("config.private.h")
  #include "config.private.h"
#endif

#ifndef CONFIG_H
#define CONFIG_H
#endif

// Safe default placeholders for public repo and initial clones.
// Any setting already defined in config.private.h will take precedence.
#ifndef MASTER_USER
  #define MASTER_USER     "user"     // Master username for web UI
#endif
#ifndef MASTER_PASS
  #define MASTER_PASS     "password" // Master password for web UI
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID       "ssid"     // Wi-Fi network SSID
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS       "password" // Wi-Fi network password
#endif
#ifndef TYPE_DELAY_MS
  #define TYPE_DELAY_MS   40
#endif
#ifndef CHORD_PRESS_MS
  #define CHORD_PRESS_MS  12         // How long a special key / chord is held before release
#endif
#ifndef UART_RX_PIN
  #define UART_RX_PIN     44
#endif
#ifndef UART_TX_PIN
  #define UART_TX_PIN     43
#endif
#ifndef UART_BAUD
  #define UART_BAUD       115200
#endif
#ifndef HOSTNAME
  #define HOSTNAME        "web-virtual-keyboard"
#endif

#ifndef FW_VERSION
  #define FW_VERSION      "1.2.0"
#endif
#ifndef STORAGE_VERSION
  #define STORAGE_VERSION "2.0"
#endif

#ifndef MAX_PRESET_LENGTH
  #define MAX_PRESET_LENGTH 64
#endif
#ifndef MAX_VALUE_LENGTH
  #define MAX_VALUE_LENGTH  512    // Preset value (literal text + <TOKEN> keys)
#endif
#ifndef MAX_JSON_SIZE
  #define MAX_JSON_SIZE     16384  // Holds all presets, adjust accordingly
#endif

#ifndef SERVER_PORT
  #define SERVER_PORT     80
#endif
#ifndef UART_NUMBER
  #define UART_NUMBER     0
#endif

#ifndef ENABLE_DISPLAY
  #define ENABLE_DISPLAY  true
#endif

#endif // CONFIG_H_WRAPPER
