#ifndef CONFIG_H
#define CONFIG_H

#define MASTER_USER      "admin"           	// <-- CHANGE THIS      
#define MASTER_PASS      "password"    		// <-- CHANGE THIS
#define WIFI_SSID        "some-wifi"		// <-- CHANGE THIS
#define WIFI_PASS        "some-password"	// <-- CHANGE THIS
#define TYPE_DELAY_MS    40
#define UART_RX_PIN      44
#define UART_TX_PIN      43
#define UART_BAUD        115200
#define HOSTNAME         "web-virtual-keyboard"

#define MAX_PRESET_LENGTH   64
#define MAX_USER_LENGTH     128      
#define MAX_PASS_LENGTH     128
#define MAX_JSON_SIZE       8192 // holds all the presets, adjust accordingly

#define SERVER_PORT     80
#define UART_NUMBER     0

#endif