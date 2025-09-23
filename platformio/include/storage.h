#ifndef STORAGE_H
#define STORAGE_H

#include <Wstring.h>

extern const char* PREFS_NS;
extern const char* PRESETS_K;

String loadPresetsJson();
bool savePresetsJson(const String& json);

#endif