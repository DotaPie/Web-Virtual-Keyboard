#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

#include "html.h"
#include "config.h"

// ---------- Globals ----------
USBHIDKeyboard Keyboard;
HardwareSerial LogSerial(0);
WebServer server(80);
Preferences prefs;

// ---------- Storage helpers (NVS JSON) ----------
static const char* PREFS_NS  = "cfg";
static const char* PRESETS_K = "presets";

// Load the JSON string (object map: name -> object) from NVS.
// Returns "{}" if not found or on error.
String loadPresetsJson()
{
	Preferences p;

	if (!p.begin(PREFS_NS, true))
	{
		return "{}";
	}

	String json = p.getString(PRESETS_K, "{}");
	p.end();

	if (json.length() == 0U)
	{
		json = "{}";
	}

	return json;
}

// Save the JSON string back to NVS.
bool savePresetsJson(const String& json)
{
	Preferences p;

	if (!p.begin(PREFS_NS, false))
	{
		return false;
	}

	bool ok = p.putString(PRESETS_K, json) > 0;
	p.end();

	return ok;
}

// Up-migrate a legacy "string value" preset into { user:"", pass:"<value>" }
static void upgradeLegacyIfNeeded(JsonVariant& v)
{
	if (v.is<const char*>())
	{
		const char* legacy = v.as<const char*>();
		JsonObject obj = v.to<JsonObject>();
		obj["user"] = "";
		obj["pass"] = legacy;
	}
}

// Insert/update a preset triplet in the JSON map stored in NVS.
// name: 1..48, user: 0..64, pass: 0..128
bool setPreset(const String& name, const String& user, const String& pass, String* errMsg = nullptr)
{
	if (name.length() == 0U || name.length() > 48U)
	{
		if (errMsg != nullptr) { *errMsg = "Bad name"; }
		return false;
	}
	if (user.length() > 64U)
	{
		if (errMsg != nullptr) { *errMsg = "Bad user"; }
		return false;
	}
	if (pass.length() > 128U)
	{
		if (errMsg != nullptr) { *errMsg = "Bad pass"; }
		return false;
	}

	const String json = loadPresetsJson();

	// 4 KB to allow a reasonable number of presets
	DynamicJsonDocument doc(4096);

	DeserializationError e = deserializeJson(doc, json);
	if (e)
	{
		if (errMsg != nullptr) { *errMsg = "JSON parse error"; }
		return false;
	}

	// Ensure object for this name, doing legacy upgrade if needed
	if (doc.containsKey(name))
	{
		JsonVariant v = doc[name];
		upgradeLegacyIfNeeded(v);
	}

	JsonObject obj = doc[name].to<JsonObject>();
	obj["user"] = user;
	obj["pass"] = pass;

	String out;
	serializeJson(doc, out);

	return savePresetsJson(out);
}

// Delete a preset by name. Returns false if absent.
bool deletePreset(const String& name)
{
	const String json = loadPresetsJson();
	DynamicJsonDocument doc(4096);

	if (deserializeJson(doc, json))
	{
		return false;
	}

	if (!doc.containsKey(name))
	{
		return false;
	}

	doc.remove(name);

	String out;
	serializeJson(doc, out);

	return savePresetsJson(out);
}

// ---------- HTTP Handlers ----------

static bool requireAuth()
{
    if (server.authenticate(MASTER_USER, MASTER_PASS)) { return true; }
    server.requestAuthentication();  // shows login popup
    return false;
}

// GET /
void handleRoot()
{
	if (!requireAuth()) { return; }

	server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

// POST /type  (form: text=..., newline=0|1|true|on)
void handleType()
{
	if (!requireAuth()) { return; }

	if (!server.hasArg("text"))
	{
		server.send(400, "text/plain", "Missing 'text'");
		return;
	}

	const String text  = server.arg("text");
	const String nlArg = server.hasArg("newline") ? server.arg("newline") : "0";
	const bool addNL   = (nlArg == "1" || nlArg == "true" || nlArg == "on");

	LogSerial.printf("[TYPE] len=%u addNL=%s | %s\r\n",
	                 static_cast<unsigned>(text.length()),
	                 addNL ? "yes" : "no",
	                 text.c_str());

	Keyboard.releaseAll();
	delay(10);

	Keyboard.print(text);

	if (addNL)
	{
		Keyboard.print("\r\n");
	}

	delay(TYPE_DELAY_MS);

	server.send(200, "text/plain", "OK");
}

// GET /presets
void handleGetPresets()
{
	if (!requireAuth()) { return; }

	// Return raw JSON; front-end handles legacy upgrade view
	const String json = loadPresetsJson();
	server.send(200, "application/json; charset=utf-8", json);
}

// POST /presets (form: name, user, pass)
void handlePostPreset()
{
	if (!requireAuth()) { return; }

	if (!server.hasArg("name"))
	{
		server.send(400, "text/plain", "Missing 'name'");
		return;
	}

	const String name = server.arg("name");
	const String user = server.hasArg("user") ? server.arg("user") : "";
	const String pass = server.hasArg("pass") ? server.arg("pass") : "";

	String err;
	if (!setPreset(name, user, pass, &err))
	{
		server.send(500, "text/plain", "Failed to save preset");
		return;
	}

	server.send(200, "text/plain", "OK");
}

// DELETE /presets?name=...
void handleDeletePreset()
{
	if (!requireAuth()) { return; }

	if (!server.hasArg("name"))
	{
		server.send(400, "text/plain", "Missing 'name'");
		return;
	}

	const String name = server.arg("name");

	if (!deletePreset(name))
	{
		server.send(404, "text/plain", "Not found");
		return;
	}

	server.send(200, "text/plain", "OK");
}

// ---------- WiFi ----------
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
	LogSerial.print("[HTTP] Server started on port 80\r\n");
}

void loop()
{
	server.handleClient();
}