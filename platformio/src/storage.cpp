#include "storage.h"
#include <Wstring.h>
#include <Preferences.h>
#include "display.h"
#include "globals.h"
#include "ArduinoJson.h"
#include "config.h"

const char* PREFS_NS  = "cfg";
const char* PRESETS_K = "presets";

uint16_t getPresetsCount()
{
    const String json = loadPresetsJson();

    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        return 0;
    }

    if (doc.is<JsonObject>()) {
        return doc.as<JsonObject>().size();
    }

    return 0;
}

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
	char temp_string[8] = "";
    sprintf(temp_string, "%d", getPresetsCount());
    display_write_word(COLOR_WHITE, Align::RIGHT, 4, temp_string);
#endif

	return ok;
}