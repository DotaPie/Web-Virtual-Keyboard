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

// Helper to convert named keys (e.g. F4, ENTER, TAB) or chords to <TOKEN> format
static String convertOperandToTokens(const String& operand)
{
	String res = "";
	int start = 0;
	int len = operand.length();

	while (start < len)
	{
		int sp = operand.indexOf(' ', start);
		String token = (sp >= 0) ? operand.substring(start, sp) : operand.substring(start);
		start = (sp >= 0) ? sp + 1 : len;

		token.trim();
		if (token.length() == 0) continue;

		// Already wrapped in <...>
		if (token.startsWith("<") && token.endsWith(">"))
		{
			res += token;
			continue;
		}

		String upper = token;
		upper.toUpperCase();

		// Function keys F1 - F12
		if (upper.length() >= 2 && upper.length() <= 3 && upper[0] == 'F' && isDigit(upper[1]))
		{
			res += "<" + upper + ">";
			continue;
		}

		// Named special keys & modifiers
		if (upper == "ENTER" || upper == "RETURN")            { res += "<ENTER>"; }
		else if (upper == "TAB")                              { res += "<TAB>"; }
		else if (upper == "ESC" || upper == "ESCAPE")         { res += "<ESC>"; }
		else if (upper == "BACKSPACE" || upper == "BKSP")     { res += "<BKSP>"; }
		else if (upper == "DEL" || upper == "DELETE")         { res += "<DEL>"; }
		else if (upper == "SPACE" || upper == "SPC")          { res += "<SPACE>"; }
		else if (upper == "UP" || upper == "UPARROW")         { res += "<UP>"; }
		else if (upper == "DOWN" || upper == "DOWNARROW")     { res += "<DOWN>"; }
		else if (upper == "LEFT" || upper == "LEFTARROW")     { res += "<LEFT>"; }
		else if (upper == "RIGHT" || upper == "RIGHTARROW")   { res += "<RIGHT>"; }
		else if (upper == "PAGEUP" || upper == "PGUP")        { res += "<PGUP>"; }
		else if (upper == "PAGEDOWN" || upper == "PGDN")      { res += "<PGDN>"; }
		else if (upper == "HOME")                             { res += "<HOME>"; }
		else if (upper == "END")                              { res += "<END>"; }
		else if (upper == "INSERT" || upper == "INS")         { res += "<INS>"; }
		else if (upper == "PRINTSCREEN" || upper == "PRTSC")  { res += "<PRTSC>"; }
		else if (upper == "SCROLLLOCK" || upper == "SCRLK")   { res += "<SCRLK>"; }
		else if (upper == "NUMLOCK" || upper == "NUMLK")      { res += "<NUMLK>"; }
		else if (upper == "PAUSE" || upper == "BREAK")        { res += "<PAUSE>"; }
		else if (upper == "CAPSLOCK" || upper == "CAPS")      { res += "<CAPS>"; }
		else if (upper == "CTRL" || upper == "CONTROL")       { res += "<CTRL>"; }
		else if (upper == "ALT")                              { res += "<ALT>"; }
		else if (upper == "SHIFT")                            { res += "<SHIFT>"; }
		else if (upper == "GUI" || upper == "WINDOWS")        { res += "<GUI>"; }
		else
		{
			// Single character or raw text
			res += token;
		}
	}
	return res;
}

// Executes a single line from a DuckyScript or macro payload
static void executePayloadLine(const String& rawLine)
{
	String line = rawLine;
	while (line.endsWith("\r") || line.endsWith("\n")) { line.remove(line.length() - 1); }
	if (line.length() == 0) { return; }

	// Comments
	if (line.startsWith("REM") || line.startsWith("//") || line.startsWith("#"))
	{
		return;
	}

	// Separate command and argument without stripping argument spaces
	int firstSpace = line.indexOf(' ');
	String cmd = (firstSpace >= 0) ? line.substring(0, firstSpace) : line;
	cmd.trim();
	cmd.toUpperCase();
	String arg = (firstSpace >= 0) ? line.substring(firstSpace + 1) : "";

	// Delay command: DELAY <ms>
	if (cmd == "DELAY")
	{
		int ms = arg.toInt();
		if (ms > 0 && ms <= 60000)
		{
			delay(ms);
		}
		return;
	}

	// Literal typing: STRING <text> (preserves trailing spaces)
	if (cmd == "STRING")
	{
		sendValue(arg);
		return;
	}

	// Literal typing with newline: STRINGLN <text>
	if (cmd == "STRINGLN")
	{
		sendValue(arg);
		sendValue("<ENTER>");
		return;
	}

	// Standalone keys
	if (cmd == "ENTER" || cmd == "RETURN")      { sendValue("<ENTER>"); return; }
	if (cmd == "ESC" || cmd == "ESCAPE")        { sendValue("<ESC>"); return; }
	if (cmd == "TAB")                           { sendValue("<TAB>"); return; }
	if (cmd == "SPACE" || cmd == "SPC")         { sendValue("<SPACE>"); return; }
	if (cmd == "BACKSPACE" || cmd == "BKSP")    { sendValue("<BKSP>"); return; }
	if (cmd == "DELETE" || cmd == "DEL")        { sendValue("<DEL>"); return; }
	if (cmd == "INSERT" || cmd == "INS")        { sendValue("<INS>"); return; }
	if (cmd == "UP" || cmd == "UPARROW")        { sendValue("<UP>"); return; }
	if (cmd == "DOWN" || cmd == "DOWNARROW")    { sendValue("<DOWN>"); return; }
	if (cmd == "LEFT" || cmd == "LEFTARROW")    { sendValue("<LEFT>"); return; }
	if (cmd == "RIGHT" || cmd == "RIGHTARROW")  { sendValue("<RIGHT>"); return; }
	if (cmd == "PAGEUP" || cmd == "PGUP")       { sendValue("<PGUP>"); return; }
	if (cmd == "PAGEDOWN" || cmd == "PGDN")     { sendValue("<PGDN>"); return; }
	if (cmd == "HOME")                          { sendValue("<HOME>"); return; }
	if (cmd == "END")                           { sendValue("<END>"); return; }
	if (cmd == "CAPSLOCK" || cmd == "CAPS")     { sendValue("<CAPS>"); return; }
	if (cmd == "PRINTSCREEN" || cmd == "PRTSC") { sendValue("<PRTSC>"); return; }
	if (cmd == "SCROLLLOCK" || cmd == "SCRLK")  { sendValue("<SCRLK>"); return; }
	if (cmd == "PAUSE" || cmd == "BREAK")       { sendValue("<PAUSE>"); return; }

	// Function keys F1 - F12
	if (cmd.length() >= 2 && cmd.length() <= 3 && cmd[0] == 'F' && isDigit(cmd[1]))
	{
		sendValue("<" + cmd + ">");
		return;
	}

	// Modifiers with named or single-char key operands
	if (cmd == "GUI" || cmd == "WINDOWS")
	{
		sendValue("<GUI>" + convertOperandToTokens(arg));
		return;
	}
	if (cmd == "CTRL" || cmd == "CONTROL")
	{
		sendValue("<CTRL>" + convertOperandToTokens(arg));
		return;
	}
	if (cmd == "ALT")
	{
		sendValue("<ALT>" + convertOperandToTokens(arg));
		return;
	}
	if (cmd == "SHIFT")
	{
		sendValue("<SHIFT>" + convertOperandToTokens(arg));
		return;
	}

	// Default fallback: parse as macro line
	sendValue(line);
}

// Background FreeRTOS task state
static TaskHandle_t s_payloadTaskHandle = NULL;
static String s_activePayloadPath = "";
static volatile bool s_payloadRunning = false;
static volatile bool s_payloadStopRequested = false;

bool isPayloadRunning()
{
	return s_payloadRunning;
}

void stopSdPayload()
{
	if (s_payloadRunning)
	{
		s_payloadStopRequested = true;
	}
}

static void payloadTaskFunc(void* param)
{
	String path = s_activePayloadPath;
	s_payloadRunning = true;
	s_payloadStopRequested = false;

	File f = SD_MMC.open(path, FILE_READ);
	if (!f || f.isDirectory())
	{
		LogSerial.printf("[SD] Payload error: Cannot open %s\r\n", path.c_str());
		if (f) f.close();
		s_payloadRunning = false;
		s_payloadTaskHandle = NULL;
		vTaskDelete(NULL);
		return;
	}

	LogSerial.printf("[SD] Starting background payload: %s\r\n", path.c_str());
	Keyboard.releaseAll();
	vTaskDelay(pdMS_TO_TICKS(50));

	String lastCommand = "";

	while (f.available() && !s_payloadStopRequested)
	{
		String line = f.readStringUntil('\n');
		while (line.endsWith("\r") || line.endsWith("\n"))
		{
			line.remove(line.length() - 1);
		}

		if (line.length() == 0)
		{
			continue;
		}

		String upper = line;
		upper.toUpperCase();
		if (upper.startsWith("REPEAT "))
		{
			int reps = upper.substring(7).toInt();
			if (reps > 0 && reps <= 100 && lastCommand.length() > 0)
			{
				for (int r = 0; r < reps && !s_payloadStopRequested; r++)
				{
					executePayloadLine(lastCommand);
					vTaskDelay(pdMS_TO_TICKS(10));
				}
			}
			continue;
		}

		lastCommand = line;
		executePayloadLine(line);
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	f.close();
	Keyboard.releaseAll();
	vTaskDelay(pdMS_TO_TICKS(20));

	LogSerial.printf("[SD] Background payload finished: %s\r\n", path.c_str());
	s_payloadRunning = false;
	s_payloadTaskHandle = NULL;
	vTaskDelete(NULL);
}

bool startSdPayload(const String& path, String* errMsg)
{
	if (!g_sdMounted)
	{
		if (errMsg != nullptr) { *errMsg = "SD card not mounted"; }
		return false;
	}

	if (s_payloadRunning)
	{
		if (errMsg != nullptr) { *errMsg = "A payload is already executing"; }
		return false;
	}

	File f = SD_MMC.open(path, FILE_READ);
	if (!f || f.isDirectory())
	{
		if (errMsg != nullptr) { *errMsg = "File not found or is a directory"; }
		if (f) f.close();
		return false;
	}
	f.close();

	s_activePayloadPath = path;
	BaseType_t res = xTaskCreatePinnedToCore(
		payloadTaskFunc,
		"sdPayload",
		8192,
		NULL,
		1,
		&s_payloadTaskHandle,
		1
	);

	if (res != pdPASS)
	{
		if (errMsg != nullptr) { *errMsg = "Failed to launch background task"; }
		return false;
	}

	return true;
}

bool runSdPayload(const String& path, String* errMsg, int* linesRun)
{
	return startSdPayload(path, errMsg);
}

