#include "sd_card.h"
#include "pinout.h"
#include "globals.h"
#include "config.h"
#include "http_handlers.h"

static bool g_sdMounted = false;
static sdcard_type_t g_cardType = CARD_NONE;

bool sdInit(bool force)
{
	if (g_sdMounted && !force)
	{
		return true;
	}

	SD_MMC.end();
	g_sdMounted = false;
	g_cardType = CARD_NONE;

	LogSerial.println("[SD] Initializing MicroSD (SD_MMC) interface...");

	// Configure pullup resistors on SD_MMC lines for stability
	pinMode(SD_MMC_CMD, INPUT_PULLUP);
	pinMode(SD_MMC_D0,  INPUT_PULLUP);
	pinMode(SD_MMC_D1,  INPUT_PULLUP);
	pinMode(SD_MMC_D2,  INPUT_PULLUP);
	pinMode(SD_MMC_D3,  INPUT_PULLUP);

	// Attempt 4-bit standard frequency (20MHz) mode first for maximum compatibility
	if (SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0, SD_MMC_D1, SD_MMC_D2, SD_MMC_D3))
	{
		if (SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT, 5))
		{
			g_cardType = SD_MMC.cardType();
			if (g_cardType != CARD_NONE)
			{
				g_sdMounted = true;
				LogSerial.printf("[SD] Mounted 4-bit bus: Type=%s, Total=%llu MB, Used=%llu MB\r\n",
				                 getSdCardTypeName().c_str(),
				                 getSdTotalBytes() / (1024ULL * 1024ULL),
				                 getSdUsedBytes()  / (1024ULL * 1024ULL));
				return true;
			}
			SD_MMC.end();
		}
	}

	LogSerial.println("[SD] 4-bit mode unsuccessful, trying 1-bit fallback...");

	// Fallback to 1-bit mode
	if (SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0))
	{
		if (SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5))
		{
			g_cardType = SD_MMC.cardType();
			if (g_cardType != CARD_NONE)
			{
				g_sdMounted = true;
				LogSerial.printf("[SD] Mounted 1-bit bus: Type=%s, Total=%llu MB\r\n",
				                 getSdCardTypeName().c_str(),
				                 getSdTotalBytes() / (1024ULL * 1024ULL));
				return true;
			}
			SD_MMC.end();
		}
	}

	LogSerial.println("[SD] No readable SD card detected.");
	return false;
}

bool isSdCardMounted()
{
	return g_sdMounted;
}

String getSdCardTypeName()
{
	if (!g_sdMounted) { return "NONE"; }
	switch (g_cardType)
	{
		case CARD_MMC:  return "MMC";
		case CARD_SD:   return "SDSC";
		case CARD_SDHC: return "SDHC";
		default:        return "UNKNOWN";
	}
}

uint64_t getSdTotalBytes()
{
	if (!g_sdMounted) { return 0; }
	return SD_MMC.totalBytes();
}

uint64_t getSdUsedBytes()
{
	if (!g_sdMounted) { return 0; }
	return SD_MMC.usedBytes();
}

uint64_t getSdFreeBytes()
{
	if (!g_sdMounted) { return 0; }
	uint64_t tot = getSdTotalBytes();
	uint64_t used = getSdUsedBytes();
	return (tot > used) ? (tot - used) : 0;
}

String getSdListJson(const String& path)
{
	if (!g_sdMounted) { return "[]"; }

	File root = SD_MMC.open(path);
	if (!root || !root.isDirectory())
	{
		return "[]";
	}

	String json = "[";
	bool first = true;

	File file = root.openNextFile();
	while (file)
	{
		if (!first) { json += ","; }
		first = false;

		String name = file.name();
		// If name starts with the directory path, strip it to get just the filename
		if (name.lastIndexOf('/') >= 0)
		{
			name = name.substring(name.lastIndexOf('/') + 1);
		}

		String fullPath = file.path();
		if (!fullPath.startsWith("/"))
		{
			fullPath = "/" + fullPath;
		}

		json += "{\"name\":\"" + name + "\",";
		json += "\"path\":\"" + fullPath + "\",";
		json += "\"size\":" + String((unsigned long)file.size()) + ",";
		json += "\"isDir\":" + String(file.isDirectory() ? "true" : "false") + "}";

		file = root.openNextFile();
	}

	json += "]";
	return json;
}

bool readSdFile(const String& path, String& content)
{
	if (!g_sdMounted) { return false; }

	File f = SD_MMC.open(path, FILE_READ);
	if (!f || f.isDirectory())
	{
		return false;
	}

	size_t sz = f.size();
	if (sz > 65536) // limit max text preview to 64KB
	{
		sz = 65536;
	}

	content.reserve(sz + 1);
	while (f.available() && content.length() < sz)
	{
		content += (char)f.read();
	}
	f.close();
	return true;
}

bool writeSdFile(const String& path, const String& content)
{
	if (!g_sdMounted) { return false; }

	File f = SD_MMC.open(path, FILE_WRITE);
	if (!f)
	{
		return false;
	}

	size_t written = f.print(content);
	f.close();
	return (written == content.length());
}

bool deleteSdPath(const String& path)
{
	if (!g_sdMounted) { return false; }

	// Try removing as file first, then as directory
	if (SD_MMC.remove(path))
	{
		return true;
	}
	return SD_MMC.rmdir(path);
}

// Executes a single line from a DuckyScript or macro payload
static void executePayloadLine(const String& rawLine)
{
	String line = rawLine;
	line.trim();
	if (line.length() == 0) { return; }

	// Comments
	if (line.startsWith("REM") || line.startsWith("//") || line.startsWith("#"))
	{
		return;
	}

	// Delay command: DELAY <ms>
	if (line.startsWith("DELAY ") || line.startsWith("delay "))
	{
		int ms = line.substring(6).toInt();
		if (ms > 0 && ms <= 60000)
		{
			delay(ms);
		}
		return;
	}

	// Literal typing: STRING <text>
	if (line.startsWith("STRING ") || line.startsWith("string "))
	{
		Keyboard.print(line.substring(7));
		delay(TYPE_DELAY_MS);
		return;
	}

	// Literal typing with newline: STRINGLN <text>
	if (line.startsWith("STRINGLN ") || line.startsWith("stringln "))
	{
		Keyboard.print(line.substring(9));
		Keyboard.print("\r\n");
		delay(TYPE_DELAY_MS);
		return;
	}

	String upper = line;
	upper.toUpperCase();

	// Standard DuckyScript standalone keys
	if (upper == "ENTER" || upper == "RETURN")      { sendValue("<ENTER>"); return; }
	if (upper == "ESC" || upper == "ESCAPE")        { sendValue("<ESC>"); return; }
	if (upper == "TAB")                             { sendValue("<TAB>"); return; }
	if (upper == "SPACE" || upper == "SPC")         { sendValue("<SPACE>"); return; }
	if (upper == "BACKSPACE" || upper == "BKSP")    { sendValue("<BKSP>"); return; }
	if (upper == "DELETE" || upper == "DEL")        { sendValue("<DEL>"); return; }
	if (upper == "INSERT" || upper == "INS")        { sendValue("<INS>"); return; }
	if (upper == "UP" || upper == "UPARROW")        { sendValue("<UP>"); return; }
	if (upper == "DOWN" || upper == "DOWNARROW")    { sendValue("<DOWN>"); return; }
	if (upper == "LEFT" || upper == "LEFTARROW")    { sendValue("<LEFT>"); return; }
	if (upper == "RIGHT" || upper == "RIGHTARROW")  { sendValue("<RIGHT>"); return; }
	if (upper == "PAGEUP" || upper == "PGUP")       { sendValue("<PGUP>"); return; }
	if (upper == "PAGEDOWN" || upper == "PGDN")     { sendValue("<PGDN>"); return; }
	if (upper == "HOME")                            { sendValue("<HOME>"); return; }
	if (upper == "END")                             { sendValue("<END>"); return; }
	if (upper == "CAPSLOCK" || upper == "CAPS")     { sendValue("<CAPS>"); return; }
	if (upper == "PRINTSCREEN" || upper == "PRTSC") { sendValue("<PRTSC>"); return; }
	if (upper == "SCROLLLOCK" || upper == "SCRLK")  { sendValue("<SCRLK>"); return; }
	if (upper == "PAUSE" || upper == "BREAK")       { sendValue("<PAUSE>"); return; }

	// Function keys F1 - F12
	if (upper.length() >= 2 && upper.length() <= 3 && upper[0] == 'F' && isDigit(upper[1]))
	{
		sendValue("<" + upper + ">");
		return;
	}

	// Common Chords
	if (upper == "CTRL ALT DEL" || upper == "CONTROL ALT DEL")
	{
		sendValue("<CTRL><ALT><DEL>");
		return;
	}
	if (upper == "CTRL SHIFT ESC" || upper == "CONTROL SHIFT ESC")
	{
		sendValue("<CTRL><SHIFT><ESC>");
		return;
	}

	// Modifiers with key: GUI / WINDOWS
	if (upper.startsWith("GUI ") || upper.startsWith("WINDOWS "))
	{
		int sp = line.indexOf(' ');
		sendValue("<GUI>" + line.substring(sp + 1));
		return;
	}
	// CTRL / CONTROL
	if (upper.startsWith("CTRL ") || upper.startsWith("CONTROL "))
	{
		int sp = line.indexOf(' ');
		sendValue("<CTRL>" + line.substring(sp + 1));
		return;
	}
	// ALT
	if (upper.startsWith("ALT "))
	{
		int sp = line.indexOf(' ');
		sendValue("<ALT>" + line.substring(sp + 1));
		return;
	}
	// SHIFT
	if (upper.startsWith("SHIFT "))
	{
		int sp = line.indexOf(' ');
		sendValue("<SHIFT>" + line.substring(sp + 1));
		return;
	}

	// Default fallback: parse as macro line (with <TOKEN> keys or literal text)
	sendValue(line);
}

bool runSdPayload(const String& path, String* errMsg, int* linesRun)
{
	if (!g_sdMounted)
	{
		if (errMsg != nullptr) { *errMsg = "SD card not mounted"; }
		return false;
	}

	File f = SD_MMC.open(path, FILE_READ);
	if (!f || f.isDirectory())
	{
		if (errMsg != nullptr) { *errMsg = "File not found or is a directory"; }
		return false;
	}

	LogSerial.printf("[SD] Running payload: %s\r\n", path.c_str());

	Keyboard.releaseAll();
	delay(20);

	int count = 0;
	String lastCommand = "";

	while (f.available())
	{
		String line = f.readStringUntil('\n');
		line.trim();

		if (line.length() == 0)
		{
			continue;
		}

		// Handle REPEAT <count>
		String upper = line;
		upper.toUpperCase();
		if (upper.startsWith("REPEAT "))
		{
			int reps = upper.substring(7).toInt();
			if (reps > 0 && reps <= 100 && lastCommand.length() > 0)
			{
				for (int r = 0; r < reps; r++)
				{
					executePayloadLine(lastCommand);
					yield();
				}
			}
			continue;
		}

		lastCommand = line;
		executePayloadLine(line);
		count++;
		yield();
	}

	f.close();
	Keyboard.releaseAll();
	delay(20);

	LogSerial.printf("[SD] Payload complete: %d lines processed\r\n", count);
	if (linesRun != nullptr) { *linesRun = count; }
	return true;
}
