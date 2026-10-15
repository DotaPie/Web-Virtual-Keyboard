#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

// Initializes the SD_MMC peripheral. Safe to call even if no card is present.
bool sdInit(bool force = false);

// Returns true if the SD card is currently mounted and ready.
bool isSdCardMounted();

// Returns human-readable card type: "SDSC", "SDHC", "MMC", "UNKNOWN", or "NONE".
String getSdCardTypeName();

// Space reporting in bytes
uint64_t getSdTotalBytes();
uint64_t getSdUsedBytes();
uint64_t getSdFreeBytes();

// Returns a JSON array string of files in the given directory path.
String getSdListJson(const String& path = "/");

// Read entire file content into string
bool readSdFile(const String& path, String& content);

// Write string content to a file on the SD card
bool writeSdFile(const String& path, const String& content);

// Delete a file or empty directory
bool deleteSdPath(const String& path);

// Executes a payload / DuckyScript / macro script from the SD card as USB keystrokes.
bool runSdPayload(const String& path, String* errMsg = nullptr, int* linesRun = nullptr);

// Non-blocking background payload execution:
bool startSdPayload(const String& path, String* errMsg = nullptr);
bool isPayloadRunning();
void stopSdPayload();

#endif // SD_CARD_H

