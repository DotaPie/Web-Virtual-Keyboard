#include "http_handlers.h"

#include <ArduinoJson.h>
#include "globals.h"
#include "config.h"
#include "storage.h"
#include "html.h" // will be generated automatically when building
#include "display.h"

// === HELPERS ===
bool setPreset(const String& name, const String& user, const String& pass, String* errMsg = nullptr)
{
	if (name.length() == 0U || name.length() > MAX_PRESET_LENGTH)
	{
		if (errMsg != nullptr) { *errMsg = "Bad name"; }
		return false;
	}
	if (user.length() > MAX_USER_LENGTH)
	{
		if (errMsg != nullptr) { *errMsg = "Bad user"; }
		return false;
	}
	if (pass.length() > MAX_PASS_LENGTH)
	{
		if (errMsg != nullptr) { *errMsg = "Bad pass"; }
		return false;
	}

	const String json = loadPresetsJson();

	DeserializationError e = deserializeJson(doc, json);
	if (e)
	{
		if (errMsg != nullptr) { *errMsg = "JSON parse error"; }
		return false;
	}

	JsonObject obj = doc[name].to<JsonObject>();
	obj["user"] = user;
	obj["pass"] = pass;

	String out;
	serializeJson(doc, out);

	return savePresetsJson(out);
}

bool deletePreset(const String& name)
{
	const String json = loadPresetsJson();
	

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

static bool requireAuth()
{
    if(server.authenticate(MASTER_USER, MASTER_PASS))
	{ 
		return true; 
	}

    server.requestAuthentication();
    return false;
}

static inline void sendCtrl(char k) {
	Keyboard.press(KEY_LEFT_CTRL);
	Keyboard.press((uint8_t)k);
	delay(8);
	Keyboard.releaseAll();
}

static inline void sendCtrlAltDel() {
	Keyboard.press(KEY_LEFT_CTRL);
	Keyboard.press(KEY_LEFT_ALT);
	Keyboard.press(KEY_DELETE);
	delay(8);
	Keyboard.releaseAll();
}

// === REST API ===

// GET /
void handleRoot()
{
	if (!requireAuth()) { return; }

	server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

// POST /type  (form: text=..., newline=0|1|true|on)
void handleType() {
	if (!requireAuth()) return;

	if (!server.hasArg("text")) {
		server.send(400, "text/plain", "Missing 'text'");
		return;
	}

	const String text  = server.arg("text");
	const String nlArg = server.hasArg("newline") ? server.arg("newline") : "0";
	const bool addNL   = (nlArg == "1" || nlArg == "true" || nlArg == "on");

	LogSerial.printf("[TYPE] len=%u addNL=%s | %s\r\n", (unsigned)text.length(), addNL ? "yes" : "no", text.c_str());

	Keyboard.releaseAll();
	delay(10);

	// --- SIMPLE hotkey mapping ---
	if (text == "\x03" || text.equalsIgnoreCase("CTRL-C") || text.equalsIgnoreCase("{CTRL+C}")) {
		sendCtrl('c');                        
	} else if (text == "\x18" || text.equalsIgnoreCase("CTRL-X") || text.equalsIgnoreCase("{CTRL+X}")) {
		sendCtrl('x');                         
	} else if (text.equalsIgnoreCase("CTRL-ALT-DEL") || text.equalsIgnoreCase("{CTRL}{ALT}{DEL}") || text.equalsIgnoreCase("{CTRL+ALT+DEL}")) {
		sendCtrlAltDel();                      
	} else {
		// Regular typing
		Keyboard.print(text);
		if (addNL) Keyboard.print("\r\n");
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