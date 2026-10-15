#ifndef STORAGE_H
#define STORAGE_H

#include <WString.h>

extern const char* PREFS_NS;
extern const char* PRESETS_K;
extern const char* VERSION_K;

// Checks the storage layout version against STORAGE_VERSION. If it differs
// (older, newer, or absent) the whole preset namespace is wiped and
// reinitialised. Call once at boot before serving any request.
void storageInit();

uint16_t getPresetsCount();
uint16_t getGroupsCount();
String loadPresetsJson();
bool savePresetsJson(const String& json);

#endif