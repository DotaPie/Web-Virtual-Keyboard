#ifndef CONFIG_H
#define CONFIG_H

#define MASTER_USER     "user"     // Master username for web UI
#define MASTER_PASS     "password" // Master password for web UI
#define WIFI_SSID       "ssid"     // Wi-Fi network SSID
#define WIFI_PASS       "password" // Wi-Fi network password
#define TYPE_DELAY_MS   40
#define CHORD_PRESS_MS  12         // How long a special key / chord is held before release
#define UART_RX_PIN     44
#define UART_TX_PIN     43
#define UART_BAUD       115200
#define HOSTNAME        "web-virtual-keyboard"

#define FW_VERSION          "1.2.0"
#define STORAGE_VERSION     "2.0"

#define MAX_PRESET_LENGTH   64
#define MAX_VALUE_LENGTH    512    // Preset value (literal text + <TOKEN> keys)
#define MAX_JSON_SIZE       16384  // Holds all presets, adjust accordingly

#define SERVER_PORT     80
#define UART_NUMBER     0

#define ENABLE_DISPLAY  true

#endif
