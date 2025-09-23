#include "storage.h"
#include <Wstring.h>
#include <Preferences.h>

const char* PREFS_NS  = "cfg";
const char* PRESETS_K = "presets";

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

	return ok;
}