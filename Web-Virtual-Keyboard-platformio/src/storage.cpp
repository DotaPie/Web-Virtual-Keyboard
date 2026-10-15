#include "storage.h"
#include <WString.h>
#include <Preferences.h>
#include "display.h"
#include "globals.h"
#include "ArduinoJson.h"
#include "config.h"

const char* PREFS_NS  = "cfg";
const char* PRESETS_K = "presets";
const char* VERSION_K = "ver";

void storageInit()
{
	Preferences p;

	if (!p.begin(PREFS_NS, false))
	{
		LogSerial.println("[NVS] begin() failed");
		return;
	}

	const String stored = p.getString(VERSION_K, "");

	if (stored != STORAGE_VERSION)
	{
		// Version mismatch (older, newer, or fresh device): wipe everything in
		// this namespace so no incompatible preset data survives. No migration.
		LogSerial.printf("[NVS] storage version \"%s\" != \"%s\" -> wiping storage\r\n",
			stored.c_str(), STORAGE_VERSION);

		p.clear();
		p.putString(VERSION_K, STORAGE_VERSION);
		p.putString(PRESETS_K, "[]");
	}
	else
	{
		// Same version: make sure the presets key exists.
		if (!p.isKey(PRESETS_K))
		{
			p.putString(PRESETS_K, "[]");
		}
		LogSerial.printf("[NVS] storage version \"%s\" OK\r\n", STORAGE_VERSION);
	}

	p.end();
}

uint16_t getPresetsCount()
{
    const String json = loadPresetsJson();

    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        return 0;
    }

    if (doc.is<JsonArray>()) {
        return doc.as<JsonArray>().size();
    }

    return 0;
}

// Counts the distinct, non-empty group names across all presets (ungrouped
// presets are ignored).
uint16_t getGroupsCount()
{
    const String json = loadPresetsJson();

    DeserializationError err = deserializeJson(doc, json);
    if (err || !doc.is<JsonArray>()) {
        return 0;
    }

    String   seen;   // wrapped group names ("\n<name>\n") for membership tests
    uint16_t count = 0;

    for (JsonObject o : doc.as<JsonArray>()) {
        const char* g = o["group"] | "";
        if (g[0] == '\0') {
            continue; // ungrouped
        }

        String key = "\n";
        key += g;
        key += "\n";

        if (seen.indexOf(key) < 0) {
            seen += key;
            count++;
        }
    }

    return count;
}

String loadPresetsJson()
{
	Preferences p;

	if (!p.begin(PREFS_NS, true))
	{
		return "[]";
	}

	String json = p.getString(PRESETS_K, "[]");
	p.end();

	if (json.length() == 0U)
	{
		json = "[]";
	}

	return json;
}

bool savePresetsJson(const String& json)
{
	Preferences p;

	if (!p.begin(PREFS_NS, false))
	{
		return false;
	}

	bool ok = p.putString(PRESETS_K, json) > 0;
	p.end();

#if ENABLE_DISPLAY
	char groups_string[8] = "";
	sprintf(groups_string, "%d", getGroupsCount());
	display_write_word(COLOR_WHITE, Align::RIGHT, 2, groups_string);

	char temp_string[8] = "";
    sprintf(temp_string, "%d", getPresetsCount());
    display_write_word(COLOR_WHITE, Align::RIGHT, 3, temp_string);
#endif

	return ok;
}