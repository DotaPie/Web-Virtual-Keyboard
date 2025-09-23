#include "http_handlers.h"

#include <ArduinoJson.h>
#include "globals.h"
#include "config.h"
#include "storage.h"
#include "html.h" // will be generated automatically when building

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

	DynamicJsonDocument doc(4096);

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

static bool requireAuth()
{
    if (server.authenticate(MASTER_USER, MASTER_PASS)) { return true; }
    server.requestAuthentication();  // shows login popup
    return false;
}

// === REST API ===

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