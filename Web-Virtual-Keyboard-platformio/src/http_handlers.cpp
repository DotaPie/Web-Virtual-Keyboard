#include "http_handlers.h"

#include <ArduinoJson.h>
#include <SD_MMC.h>
#include "sd_card.h"
#include "globals.h"
#include "config.h"
#include "storage.h"
#include "html.h" // will be generated automatically when building
#include "display.h"

// === SPECIAL KEY TABLE ===
// Preset values are literal text with embedded <TOKEN> keys. Each token maps to
// a raw USB HID usage code so we can drive it uniformly through pressRaw() /
// releaseRaw() (the plain KEY_* macros don't cover ScrLk / PrtSc / NumLk / ...).
// Modifiers use usage 0xE0-0xE3 and are held for the *next* key (chord model).
struct KeyDef
{
	const char* name;
	uint8_t     usage;
	bool        isMod;
};

static const KeyDef KEY_DEFS[] =
{
	// modifiers (held for the following key)
	{ "CTRL",        0xE0, true  }, { "CONTROL",     0xE0, true  },
	{ "SHIFT",       0xE1, true  },
	{ "ALT",         0xE2, true  },
	{ "WIN",         0xE3, true  }, { "GUI",         0xE3, true  },
	{ "WINDOWS",     0xE3, true  }, { "META",        0xE3, true  },

	// editing / whitespace
	{ "ENTER",       0x28, false }, { "RETURN",      0x28, false },
	{ "ESC",         0x29, false }, { "ESCAPE",      0x29, false },
	{ "BKSP",        0x2A, false }, { "BACKSPACE",   0x2A, false }, { "BS", 0x2A, false },
	{ "TAB",         0x2B, false },
	{ "SPACE",       0x2C, false }, { "SPC",         0x2C, false },

	// locks / system
	{ "CAPS",        0x39, false }, { "CAPSLOCK",    0x39, false },
	{ "PRTSC",       0x46, false }, { "PRINTSCREEN", 0x46, false }, { "PRTSCR", 0x46, false },
	{ "SCRLK",       0x47, false }, { "SCROLLLOCK",  0x47, false },
	{ "PAUSE",       0x48, false }, { "BREAK",       0x48, false },
	{ "NUMLK",       0x53, false }, { "NUMLOCK",     0x53, false },
	{ "MENU",        0x65, false }, { "APP",         0x65, false }, { "APPLICATION", 0x65, false },

	// navigation
	{ "INS",         0x49, false }, { "INSERT",      0x49, false },
	{ "HOME",        0x4A, false },
	{ "PGUP",        0x4B, false }, { "PAGEUP",      0x4B, false },
	{ "DEL",         0x4C, false }, { "DELETE",      0x4C, false },
	{ "END",         0x4D, false },
	{ "PGDN",        0x4E, false }, { "PAGEDOWN",    0x4E, false },
	{ "RIGHT",       0x4F, false },
	{ "LEFT",        0x50, false },
	{ "DOWN",        0x51, false },
	{ "UP",          0x52, false },

	// function keys
	{ "F1",  0x3A, false }, { "F2",  0x3B, false }, { "F3",  0x3C, false },
	{ "F4",  0x3D, false }, { "F5",  0x3E, false }, { "F6",  0x3F, false },
	{ "F7",  0x40, false }, { "F8",  0x41, false }, { "F9",  0x42, false },
	{ "F10", 0x43, false }, { "F11", 0x44, false }, { "F12", 0x45, false },
};

static const KeyDef* lookupKey(const String& upperName)
{
	for (const KeyDef& kd : KEY_DEFS)
	{
		if (upperName.equals(kd.name))
		{
			return &kd;
		}
	}
	return nullptr;
}

// Types a preset value using a "hold-until-next-literal" chord model: every
// recognised <TOKEN> special key (modifiers *and* keys/navigation/system/
// function) is pressed and held as it is read. Held keys keep accumulating until
// the next normal character, which is pressed together with them and then the
// whole chord is released at once. If the string ends while keys are still held,
// they are pressed and released together. Examples:
//   "<CTRL><ALT><DEL>" -> hold Ctrl+Alt+Del, released together (real Ctrl+Alt+Del)
//   "<CTRL>c"          -> Ctrl+C
//   "<F1><F2>ca"       -> F1+F2+c chord, then "a" typed normally
void sendValue(const String& v)
{
	uint8_t held[16];   // raw HID usages currently held (modifiers + specials)
	uint8_t nHeld = 0;
	String  lit;        // pending normal text (typed as-is, never part of a chord)

	// Type any pending normal text (only reached when nothing is held).
	auto flushLiteral = [&]()
	{
		if (lit.length() == 0)
		{
			return;
		}
		Keyboard.print(lit);
		lit = "";
		delay(TYPE_DELAY_MS);
	};

	// Release the held chord. If triggerChar >= 0 it is pressed together with the
	// held keys (the chord's final key) before everything is released.
	auto releaseHeld = [&](int triggerChar)
	{
		if (nHeld == 0)
		{
			if (triggerChar >= 0) { lit += (char)triggerChar; }
			return;
		}

		for (uint8_t i = 0; i < nHeld; i++)
		{
			Keyboard.pressRaw(held[i]);
		}
		if (triggerChar >= 0)
		{
			Keyboard.press((uint8_t)triggerChar);
		}
		delay(CHORD_PRESS_MS);
		Keyboard.releaseAll();
		nHeld = 0;
		delay(TYPE_DELAY_MS);
	};

	const int n = v.length();
	int i = 0;

	while (i < n)
	{
		const char c = v[i];

		if (c == '<')
		{
			const int close = v.indexOf('>', i + 1);
			if (close > i)
			{
				String name = v.substring(i + 1, close);
				name.trim();
				name.toUpperCase();

				// Inline delay support: <DELAY 500> or <WAIT:1000>
				if (name.startsWith("DELAY ") || name.startsWith("DELAY:") ||
				    name.startsWith("WAIT ")  || name.startsWith("WAIT:"))
				{
					flushLiteral();
					int sp = name.indexOf(' ');
					if (sp < 0) { sp = name.indexOf(':'); }
					int ms = name.substring(sp + 1).toInt();
					if (ms > 0 && ms <= 60000)
					{
						delay(ms);
					}
					i = close + 1;
					continue;
				}

				const KeyDef* kd = lookupKey(name);
				if (kd != nullptr)
				{
					// A recognised special key: hold it. Pending normal text is
					// typed first (it is not part of the upcoming chord).
					flushLiteral();
					if (nHeld < (uint8_t)sizeof(held))
					{
						held[nHeld++] = kd->usage;
					}
					i = close + 1;
					continue;
				}
			}
			// Not a recognised <TOKEN>: fall through and treat '<' as literal.
		}

		// A normal character. If keys are held it closes the chord; otherwise it
		// is buffered as plain text.
		if (nHeld > 0)
		{
			releaseHeld((int)(uint8_t)c);
		}
		else
		{
			lit += c;
		}
		i++;
	}

	flushLiteral();

	// Trailing held keys with no following character: press + release together.
	if (nHeld > 0)
	{
		releaseHeld(-1);
	}
}

// === HELPERS ===
static bool validVis(const String& vis)
{
	return vis == "plain" || vis == "mask" || vis == "hidden";
}

// Finds the index of the preset identified by (name, group) in the parsed
// presets array (global `doc`), or -1 if none exists. Presets are keyed by the
// (name, group) pair, so the same name may appear in different groups.
static int findPresetIndex(const String& name, const String& group)
{
	if (!doc.is<JsonArray>())
	{
		return -1;
	}

	JsonArray arr = doc.as<JsonArray>();
	for (size_t i = 0; i < arr.size(); i++)
	{
		JsonObject o = arr[i].as<JsonObject>();
		if (name == (o["name"] | "") && group == (o["group"] | ""))
		{
			return (int)i;
		}
	}
	return -1;
}

// Create or update a preset, identified by (name, group). On edit (isEdit) the
// original identity is (oldName, oldGroup) and the name and/or group may change.
// A blank value on edit keeps the stored value (used for hidden presets, which
// never send their value back from the browser). Names must be unique within a
// group but may repeat across different groups.
bool setPreset(const String& name, const String& group,
               const String& oldName, const String& oldGroup, bool isEdit,
               const String& value, bool hasValue, const String& vis, String* errMsg = nullptr)
{
	if (name.length() == 0U || name.length() > MAX_PRESET_LENGTH)
	{
		if (errMsg != nullptr) { *errMsg = "Bad name"; }
		return false;
	}
	if (hasValue && value.length() > MAX_VALUE_LENGTH)
	{
		if (errMsg != nullptr) { *errMsg = "Value too long"; }
		return false;
	}
	if (group.length() > MAX_PRESET_LENGTH)
	{
		if (errMsg != nullptr) { *errMsg = "Group too long"; }
		return false;
	}
	if (!validVis(vis))
	{
		if (errMsg != nullptr) { *errMsg = "Bad visibility"; }
		return false;
	}

	const String json = loadPresetsJson();

	DeserializationError e = deserializeJson(doc, json);
	if (e)
	{
		if (errMsg != nullptr) { *errMsg = "JSON parse error"; }
		return false;
	}
	if (!doc.is<JsonArray>())
	{
		doc.to<JsonArray>();
	}

	// The preset being edited (if any), by its original identity.
	const int editIdx = isEdit ? findPresetIndex(oldName, oldGroup) : -1;

	// Enforce name uniqueness within a group: reject a target (name, group) that
	// already belongs to a *different* preset.
	const int clashIdx = findPresetIndex(name, group);
	if (clashIdx >= 0 && clashIdx != editIdx)
	{
		if (errMsg != nullptr) { *errMsg = "A preset with this name already exists in this group"; }
		return false;
	}

	JsonObject obj;
	if (editIdx >= 0)
	{
		obj = doc.as<JsonArray>()[editIdx].as<JsonObject>();
	}
	else
	{
		obj = doc.as<JsonArray>().add<JsonObject>();
		obj["value"] = ""; // new preset starts empty unless a value is supplied
	}

	if (hasValue)
	{
		obj["value"] = value;
	}
	obj["name"]  = name;
	obj["group"] = group;
	obj["vis"]   = vis;

	String out;
	serializeJson(doc, out);

	return savePresetsJson(out);
}

bool deletePreset(const String& name, const String& group)
{
	const String json = loadPresetsJson();

	if (deserializeJson(doc, json))
	{
		return false;
	}

	const int idx = findPresetIndex(name, group);
	if (idx < 0)
	{
		return false;
	}

	doc.as<JsonArray>().remove(idx);

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

// === REST API ===

// GET /
void handleRoot()
{
	if (!requireAuth()) { return; }

	server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

// POST /type  (form: text=..., newline=0|1|true|on)
// Free-text typing from the main textarea. No token parsing here.
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

	Keyboard.print(text);
	if (addNL) { Keyboard.print("\r\n"); }

	delay(TYPE_DELAY_MS);
	server.send(200, "text/plain", "OK");
}

// GET /info
// Small metadata endpoint so the web UI footer can show the firmware and storage
// layout versions (compiled-in constants, not stored data).
void handleGetInfo()
{
	if (!requireAuth()) { return; }

	String out = "{\"fw\":\"";
	out += FW_VERSION;
	out += "\",\"storage\":\"";
	out += STORAGE_VERSION;
	out += "\"}";

	server.send(200, "application/json; charset=utf-8", out);
}

// GET /presets
// Returns [ { name, group, vis, value? } ]. Values of "hidden" presets are
// stripped so they never reach the browser.
void handleGetPresets()
{
	if (!requireAuth()) { return; }

	const String json = loadPresetsJson();

	if (deserializeJson(doc, json))
	{
		server.send(200, "application/json; charset=utf-8", "[]");
		return;
	}

	DynamicJsonDocument out(MAX_JSON_SIZE);
	JsonArray root = out.to<JsonArray>();

	if (doc.is<JsonArray>())
	{
		for (JsonObject src : doc.as<JsonArray>())
		{
			String vis = src["vis"] | "mask";

			JsonObject dst = root.add<JsonObject>();
			dst["name"]  = src["name"]  | "";
			dst["group"] = src["group"] | "";
			dst["vis"]   = vis;

			if (vis != "hidden")
			{
				dst["value"] = src["value"] | "";
			}
		}
	}

	String outStr;
	serializeJson(out, outStr);
	server.send(200, "application/json; charset=utf-8", outStr);
}

// POST /presets (form: name, group, vis, [value], [oldName, oldGroup])
// Presence of 'oldName' marks an edit; the (oldName, oldGroup) pair identifies
// the preset to update, and name/group may change. If 'value' is omitted the
// existing value is kept (used when editing a hidden preset without retyping it).
void handlePostPreset()
{
	if (!requireAuth()) { return; }

	if (!server.hasArg("name"))
	{
		server.send(400, "text/plain", "Missing 'name'");
		return;
	}

	const String name     = server.arg("name");
	const bool   hasValue = server.hasArg("value");
	const String value    = hasValue ? server.arg("value") : String("");
	const String vis      = server.hasArg("vis") ? server.arg("vis") : String("mask");
	const String group    = server.hasArg("group") ? server.arg("group") : String("");
	const bool   isEdit   = server.hasArg("oldName");
	const String oldName  = isEdit ? server.arg("oldName") : String("");
	const String oldGroup = server.hasArg("oldGroup") ? server.arg("oldGroup") : String("");

	String err;
	if (!setPreset(name, group, oldName, oldGroup, isEdit, value, hasValue, vis, &err))
	{
		server.send(400, "text/plain", err.length() ? err : String("Failed to save preset"));
		return;
	}

	server.send(200, "text/plain", "OK");
}

// DELETE /presets?name=...&group=...
void handleDeletePreset()
{
	if (!requireAuth()) { return; }

	if (!server.hasArg("name"))
	{
		server.send(400, "text/plain", "Missing 'name'");
		return;
	}

	const String name  = server.arg("name");
	const String group = server.hasArg("group") ? server.arg("group") : String("");

	if (!deletePreset(name, group))
	{
		server.send(404, "text/plain", "Not found");
		return;
	}

	server.send(200, "text/plain", "OK");
}

// POST /send (form: name, [group])
// Types a stored preset identified by (name, group). The value (incl. hidden
// ones) never leaves the device; token parsing + chords happen here.
void handleSendPreset()
{
	if (!requireAuth()) { return; }

	if (!server.hasArg("name"))
	{
		server.send(400, "text/plain", "Missing 'name'");
		return;
	}

	const String name  = server.arg("name");
	const String group = server.hasArg("group") ? server.arg("group") : String("");
	const String json  = loadPresetsJson();

	if (deserializeJson(doc, json))
	{
		server.send(500, "text/plain", "JSON parse error");
		return;
	}

	const int idx = findPresetIndex(name, group);
	if (idx < 0)
	{
		server.send(404, "text/plain", "Not found");
		return;
	}

	const String value = doc.as<JsonArray>()[idx]["value"] | "";

	LogSerial.printf("[SEND] preset \"%s\" len=%u\r\n", name.c_str(), (unsigned)value.length());

	Keyboard.releaseAll();
	delay(10);
	sendValue(value);
	delay(TYPE_DELAY_MS);

	server.send(200, "text/plain", "OK");
}

// === MICROSD HANDLERS ===

// GET /sd/status[?refresh=1]
void handleSdStatus()
{
	if (!requireAuth()) { return; }

	if (!isSdCardMounted() || server.hasArg("refresh"))
	{
		sdInit(true);
	}

	String json = "{";
	json += "\"mounted\":" + String(isSdCardMounted() ? "true" : "false") + ",";
	json += "\"type\":\"" + getSdCardTypeName() + "\",";
	json += "\"totalMB\":" + String((unsigned long)(getSdTotalBytes() / (1024ULL * 1024ULL))) + ",";
	json += "\"usedMB\":" + String((unsigned long)(getSdUsedBytes() / (1024ULL * 1024ULL))) + ",";
	json += "\"freeMB\":" + String((unsigned long)(getSdFreeBytes() / (1024ULL * 1024ULL)));
	json += "}";

	server.send(200, "application/json; charset=utf-8", json);
}

// GET /sd/list?path=/
void handleSdList()
{
	if (!requireAuth()) { return; }

	if (!isSdCardMounted())
	{
		server.send(400, "application/json", "{\"error\":\"SD card not mounted\"}");
		return;
	}

	String path = server.hasArg("path") ? server.arg("path") : "/";
	if (!path.startsWith("/")) { path = "/" + path; }

	String json = getSdListJson(path);
	server.send(200, "application/json; charset=utf-8", json);
}

// GET /sd/file?path=/...[&download=1]
void handleSdFile()
{
	if (!requireAuth()) { return; }

	if (!isSdCardMounted())
	{
		server.send(400, "text/plain", "SD card not mounted");
		return;
	}

	if (!server.hasArg("path"))
	{
		server.send(400, "text/plain", "Missing 'path'");
		return;
	}

	String path = server.arg("path");
	if (!path.startsWith("/")) { path = "/" + path; }

	File f = SD_MMC.open(path, FILE_READ);
	if (!f || f.isDirectory())
	{
		server.send(404, "text/plain", "File not found");
		return;
	}

	String contentType = "text/plain";
	if (server.hasArg("download"))
	{
		contentType = "application/octet-stream";
		String filename = path.substring(path.lastIndexOf('/') + 1);
		server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
	}

	server.streamFile(f, contentType);
	f.close();
}

// Upload chunk handler
static File sdUploadFile;

void handleSdUpload()
{
	if (!requireAuth()) { return; }
	server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleSdUploadChunk()
{
	if (!server.authenticate(MASTER_USER, MASTER_PASS))
	{
		if (sdUploadFile) { sdUploadFile.close(); }
		return;
	}

	HTTPUpload& upload = server.upload();
	if (upload.status == UPLOAD_FILE_START)
	{
		if (!isSdCardMounted()) { return; }

		String filename = upload.filename;
		if (!filename.startsWith("/")) { filename = "/" + filename; }

		LogSerial.printf("[SD] Upload start: %s\r\n", filename.c_str());
		sdUploadFile = SD_MMC.open(filename, FILE_WRITE);
	}
	else if (upload.status == UPLOAD_FILE_WRITE)
	{
		if (sdUploadFile)
		{
			sdUploadFile.write(upload.buf, upload.currentSize);
		}
	}
	else if (upload.status == UPLOAD_FILE_END)
	{
		if (sdUploadFile)
		{
			sdUploadFile.close();
			LogSerial.printf("[SD] Upload finished: %u bytes\r\n", (unsigned)upload.totalSize);
		}
	}
}

// POST /sd/save (form: path, content)
void handleSdSave()
{
	if (!requireAuth()) { return; }

	if (!isSdCardMounted())
	{
		server.send(400, "text/plain", "SD card not mounted");
		return;
	}

	if (!server.hasArg("path") || !server.hasArg("content"))
	{
		server.send(400, "text/plain", "Missing path or content");
		return;
	}

	String path = server.arg("path");
	if (!path.startsWith("/")) { path = "/" + path; }
	String content = server.arg("content");

	if (writeSdFile(path, content))
	{
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	}
	else
	{
		server.send(500, "text/plain", "Failed to save file");
	}
}

// POST /sd/delete (form: path)
void handleSdDelete()
{
	if (!requireAuth()) { return; }

	if (!isSdCardMounted())
	{
		server.send(400, "text/plain", "SD card not mounted");
		return;
	}

	if (!server.hasArg("path"))
	{
		server.send(400, "text/plain", "Missing 'path'");
		return;
	}

	String path = server.arg("path");
	if (!path.startsWith("/")) { path = "/" + path; }

	if (deleteSdPath(path))
	{
		server.send(200, "application/json", "{\"status\":\"ok\"}");
	}
	else
	{
		server.send(400, "text/plain", "Failed to delete file or directory");
	}
}

// POST /sd/run (form: path)
void handleSdRun()
{
	if (!requireAuth()) { return; }

	if (!isSdCardMounted())
	{
		server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"SD card not mounted\"}");
		return;
	}

	if (!server.hasArg("path"))
	{
		server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing path\"}");
		return;
	}

	String path = server.arg("path");
	if (!path.startsWith("/")) { path = "/" + path; }

	String err;
	int lines = 0;
	if (runSdPayload(path, &err, &lines))
	{
		String res = "{\"status\":\"ok\",\"lines\":" + String(lines) + "}";
		server.send(200, "application/json", res);
	}
	else
	{
		String res = "{\"status\":\"error\",\"message\":\"" + err + "\"}";
		server.send(400, "application/json", res);
	}
}

