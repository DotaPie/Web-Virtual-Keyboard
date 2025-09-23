#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "storage.h"
#include "globals.h"
#include "http_handlers.h"

bool connectWiFi()
{
	LogSerial.printf("[WiFi] Connecting to \"%s\"...", WIFI_SSID);

	WiFi.setHostname(HOSTNAME);
	WiFi.mode(WIFI_MODE_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASS);

	const uint32_t start = millis();

	while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000U)
	{
		delay(250);
		LogSerial.print('.');
	}

	LogSerial.print("\r\n");

	if (WiFi.status() == WL_CONNECTED)
	{
		LogSerial.printf("[WiFi] OK: %s\r\n", WiFi.localIP().toString().c_str());
		return true;
	}

	LogSerial.print("[WiFi] FAILED (timeout).\r\n");
	return false;
}

// ---------- Setup / Loop ----------
void setup()
{
	// UART for logging over an external USB-UART adapter (not CDC)
	LogSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
	delay(100);
	LogSerial.print("\r\n=== Web Keyboard Server ===\r\n");

	USB.begin();
	Serial.begin(115200);
	delay(200);
	Keyboard.begin();
	LogSerial.print("[USB] HID Keyboard ready\r\n");
	delay(200);

	(void)connectWiFi();

	// Initialize NVS key if absent
	prefs.begin(PREFS_NS, false);
	if (!prefs.isKey(PRESETS_K))
	{
		prefs.putString(PRESETS_K, "{}");
		LogSerial.println("[NVS] Initialized empty presets");
	}
	prefs.end();

	// HTTP routes
	server.on("/", HTTP_GET, handleRoot);
	server.on("/type", HTTP_POST, handleType);
	server.on("/presets", HTTP_GET, handleGetPresets);
	server.on("/presets", HTTP_POST, handlePostPreset);
	server.on("/presets", HTTP_DELETE, handleDeletePreset);

	server.on("/favicon.ico", HTTP_GET, []()
	{
		server.send(204);
	});

	server.begin();
	LogSerial.print("[HTTP] Server started on port ");
	LogSerial.println(SERVER_PORT);
}

void loop()
{
	server.handleClient();
}