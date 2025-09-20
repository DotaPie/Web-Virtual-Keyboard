#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "USB.h"
#include "USBHIDKeyboard.h"

// ============================================================================
// Web Virtual Keyboard (ESP32-Sx + USB HID)
// - Serves a small AJAX web UI
// - Types received text over USB as a HID keyboard
// - Supports named "presets" persisted in NVS (JSON map)
//   Schema (new): { "<name>": { "user": "<u>", "pass": "<p>" }, ... }
//   Back-compat: old string values become { user:"", pass:"<old>" }
// Formatting rules applied:
//   * Allman braces style
//   * Tabs (visual width 4)
//   * Curly braces for single-line if/for
// ============================================================================

// ---------- Config ----------
#define WIFI_SSID        "name"
#define WIFI_PASS        "pass"
#define TYPE_DELAY_MS    40
#define UART_RX_PIN      44
#define UART_TX_PIN      43
#define UART_BAUD        115200
#define HOSTNAME         "web-virtual-keyboard"

// ---------- Globals ----------
USBHIDKeyboard Keyboard;
HardwareSerial LogSerial(0);
WebServer server(80);
Preferences prefs;

// ---------- Storage helpers (NVS JSON) ----------
static const char* PREFS_NS  = "cfg";
static const char* PRESETS_K = "presets";

// Load the JSON string (object map: name -> object) from NVS.
// Returns "{}" if not found or on error.
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

// Save the JSON string back to NVS.
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

// Up-migrate a legacy "string value" preset into { user:"", pass:"<value>" }
static void upgradeLegacyIfNeeded(JsonVariant& v)
{
	if (v.is<const char*>())
	{
		const char* legacy = v.as<const char*>();
		JsonObject obj = v.to<JsonObject>();
		obj["user"] = "";
		obj["pass"] = legacy;
	}
}

// Insert/update a preset triplet in the JSON map stored in NVS.
// name: 1..48, user: 0..64, pass: 0..128
bool setPreset(const String& name, const String& user, const String& pass, String* errMsg = nullptr)
{
	if (name.length() == 0U || name.length() > 48U)
	{
		if (errMsg != nullptr) { *errMsg = "Bad name"; }
		return false;
	}
	if (user.length() > 64U)
	{
		if (errMsg != nullptr) { *errMsg = "Bad user"; }
		return false;
	}
	if (pass.length() > 128U)
	{
		if (errMsg != nullptr) { *errMsg = "Bad pass"; }
		return false;
	}

	const String json = loadPresetsJson();

	// 4 KB to allow a reasonable number of presets
	DynamicJsonDocument doc(4096);

	DeserializationError e = deserializeJson(doc, json);
	if (e)
	{
		if (errMsg != nullptr) { *errMsg = "JSON parse error"; }
		return false;
	}

	// Ensure object for this name, doing legacy upgrade if needed
	if (doc.containsKey(name))
	{
		JsonVariant v = doc[name];
		upgradeLegacyIfNeeded(v);
	}

	JsonObject obj = doc[name].to<JsonObject>();
	obj["user"] = user;
	obj["pass"] = pass;

	String out;
	serializeJson(doc, out);

	return savePresetsJson(out);
}

// Delete a preset by name. Returns false if absent.
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

// ---------- HTML (AJAX) ----------
const char* PAGE_HTML = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Web Virtual Keyboard</title>

<style>
:root {
	--bg: #121315;
	--text: #f1f3f5;
	--muted: #a8acb3;
	--card: #1b1c20;
	--border: #2b2d33;

	--accent-1: #ff7a18;
	--accent-2: #ffa24d;

	--ok: #9de19d;
	--err: #ff8a8a;
	--warn: #f1c40f;

	--radius: 14px;
	--radius-sm: 10px;

	--gap: 10px;
	--pad: 18px;

	--fs-body: 16px;
	--fs-small: 14px;
}

* { box-sizing: border-box; }

body {
	margin: 0;
	background: var(--bg);
	color: var(--text);
	font-family: system-ui, -apple-system, Segoe UI, Roboto, Ubuntu, Arial, sans-serif;
	display: flex;
	justify-content: center;
	padding: 28px 16px;
}

h1 { margin: 0 0 14px; font-size: clamp(20px, 3.6vw, 32px); }
h2 { margin: 18px 0 10px; font-size: 18px; color: var(--muted); font-weight: 700; letter-spacing: .3px; }

.card {
	width: min(1200px, 95vw);
	background: var(--card);
	border: 1px solid var(--border);
	border-radius: var(--radius);
	padding: var(--pad);
}

.row { display: flex; flex-wrap: wrap; align-items: center; gap: var(--gap); }
.row + .row { margin-top: 10px; }

/* top input + Send button as grid */
.row--send {
	display: grid;
	grid-template-columns: 1fr auto;
	gap: var(--gap);
	align-items: stretch;
}

/* 3 inputs + button on one line for desktop */
.row--form {
	display: grid;
	grid-template-columns:
		minmax(260px, 2fr)   /* preset name  */
		minmax(200px, 1.5fr) /* username     */
		minmax(220px, 1.5fr) /* password     */
		auto;                /* button       */
	gap: var(--gap);
	align-items: stretch;
}

input[type="text"], input[type="password"] {
	height: 48px;
	padding: 10px 14px;
	font-size: var(--fs-body);
	color: var(--text);
	background: #141519;
	border: 1px solid var(--border);
	border-radius: var(--radius-sm);
	outline: none;
	transition: border-color .15s, box-shadow .15s;
	min-width: 0;
	width: 100%;
}

input[type="text"]:focus, input[type="text"]:hover,
input[type="password"]:focus, input[type="password"]:hover {
	border-color: var(--accent-1);
	box-shadow: 0 0 8px 2px rgba(255, 122, 24, .35);
}

input[type="checkbox"] { width: 20px; height: 20px; accent-color: var(--accent-1); cursor: pointer; }
.checkbox { display: inline-flex; align-items: center; gap: 8px; }

button {
	height: 48px; padding: 0 20px; border: 0; border-radius: var(--radius-sm);
	font-weight: 700; color: #101114; cursor: pointer;
	background: linear-gradient(180deg, var(--accent-1) 0%, var(--accent-2) 100%);
	transition: filter .15s, box-shadow .15s;
}

button:hover { filter: brightness(1.15); box-shadow: 0 0 14px 4px rgba(255,122,24,.6); }
button:active{ filter: brightness(1.05); }
button:disabled{ opacity: .65; cursor: not-allowed; }

.hr { height: 1px; background: var(--border); margin: 16px 0; }
.small { font-size: var(--fs-small); color: var(--muted); }
#msg { margin-top: 10px; font-size: var(--fs-small); color: var(--muted); min-height: 1.2em; }
#msg.ok{ color: var(--ok); } #msg.err{ color: var(--err);} #msg.warn{ color: var(--warn); }

.kv {
	display: grid;
	grid-template-columns: minmax(120px, 220px) /* name */ 1fr /* values */;
	gap: 16px;
	align-items: center;
}
.kv .name { font-weight: 600; color: var(--text); word-break: break-word; }
.kv .vals {
	display: grid;
	grid-template-columns: 1fr 1fr max-content; /* user | pass | actions */
	gap: 12px;
	align-items: center;
	width: 100%;
}

.val {
	font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace;
	white-space: nowrap;
	overflow: hidden;
	text-overflow: ellipsis;
	min-width: 0; /* required for ellipsis inside grid */
}
.kv + .kv { border-top: 1px dashed var(--border); padding-top: 10px; margin-top: 10px; }

.btn-sm { height: 38px; padding: 0 14px; border-radius: 10px; }
.toggle {
	display: inline-block;
	text-decoration: underline;
	cursor: pointer;
	font-size: var(--fs-small);
	color: var(--muted);
	width: 4ch;           /* reserves space for “show”/“hide” */
	text-align: left;
}
</style>
</head>

<body>
<div class="card">
	<h1>Web Virtual Keyboard</h1>

	<!-- Wide text box + Send -->
	<div class="row row--send">
		<input id="text" type="text" placeholder="Enter text here" autocomplete="off">
		<button id="send" type="button">Send</button>
	</div>

	<div class="row">
		<label class="checkbox" for="appendNL">
			<input type="checkbox" id="appendNL" checked>
			<span>Auto press Enter</span>
		</label>
	</div>

	<div class="hr"></div>

	<h2>Manage Presets</h2>
	<div class="row row--form">
		<input id="pname" type="text" placeholder="Preset name" autocomplete="off">
		<input id="puser" type="text" placeholder="Username" autocomplete="off">
		<input id="ppass" type="password" placeholder="Password" autocomplete="new-password">
		<button id="addPreset" type="button">Add / Update</button>
	</div>

	<div class="row small">
		Presets are saved persistently in flash memory (username + password).
	</div>

	<div class="hr"></div>

	<!-- Presets list -->
	<div id="plist"></div>

	<div id="msg" aria-live="polite"></div>
</div>

<script>
const text   = document.getElementById("text");
const msg    = document.getElementById("msg");
const btn    = document.getElementById("send");
const cbNL   = document.getElementById("appendNL");
const pname  = document.getElementById("pname");
const puser  = document.getElementById("puser");
const ppass  = document.getElementById("ppass");
const addBtn = document.getElementById("addPreset");
const plist  = document.getElementById("plist");

function setMsg(t, cls) {
	msg.textContent = t;
	msg.className   = cls || "";
}

async function sendText(value) {
	const newlineFlag = cbNL.checked ? "1" : "0";
	const body        = "text=" + encodeURIComponent(value) + "&newline=" + newlineFlag;

	setMsg("Sending ...", "");
	btn.disabled = true;

	try {
		const r = await fetch("/type", {
			method: "POST",
			headers: { "Content-Type": "application/x-www-form-urlencoded" },
			body
		});
		if (!r.ok) { throw new Error(); }
		setMsg("Text sent", "ok");
	} catch (e) {
		setMsg("Sending text failed", "err");
	} finally {
		btn.disabled = false;
	}
}

btn.addEventListener("click", function() {
	const value = text.value.trim();
	if (!value) { setMsg("Enter something to send", "warn"); return; }
	sendText(value);
});

function mask(s) {
	if (!s) { return ""; }
	return "•".repeat(Math.min(s.length, 12)) + (s.length > 12 ? "…" : "");
}

function renderList(presets) {
	plist.innerHTML = "";

	const names = Object.keys(presets).sort(function(a, b) { return a.localeCompare(b); });

	if (names.length === 0) {
		plist.innerHTML = '<div class="small">No presets yet</div>';
		return;
	}

	for (const n of names) {
		let entry = presets[n];

		// Back-compat: legacy string value becomes { user:"", pass:"<string>" }
		if (typeof entry === "string") {
			entry = { user: "", pass: entry };
		}

		const row = document.createElement("div");
		row.className = "kv";

		const nameEl = document.createElement("div");
		nameEl.className = "name";
		nameEl.textContent = n;

		const vals = document.createElement("div");
		vals.className = "vals";

		const userEl = document.createElement("div");
		userEl.className = "val small";
		userEl.textContent = entry.user || "";

		const passEl = document.createElement("div");
		passEl.className = "val small";
		let shown = false;
		const passText = document.createElement("span");
		passText.textContent = mask(entry.pass || "");
		const toggle = document.createElement("span");
		toggle.className = "toggle";
		toggle.textContent = "show";
		toggle.addEventListener("click", function() {
			shown = !shown;
			passText.textContent = shown ? (entry.pass || "") : mask(entry.pass || "");
			toggle.textContent = shown ? "hide" : "show";
		});
		passEl.appendChild(passText);
		passEl.appendChild(document.createTextNode(" "));
		passEl.appendChild(toggle);

		const actions = document.createElement("div");
		actions.style.display = "flex";
		actions.style.gap = "10px";

		const sendUserBtn = document.createElement("button");
		sendUserBtn.className = "btn-sm";
		sendUserBtn.textContent = "Send User";
		sendUserBtn.addEventListener("click", function() {
			if (!entry.user) { setMsg("Empty username", "warn"); return; }
			text.value = entry.user;
			sendText(entry.user);
		});

		const sendPassBtn = document.createElement("button");
		sendPassBtn.className = "btn-sm";
		sendPassBtn.textContent = "Send Pass";
		sendPassBtn.addEventListener("click", function() {
			if (!entry.pass) { setMsg("Empty password", "warn"); return; }
			text.value = entry.pass;
			sendText(entry.pass);
		});

		const delBtn = document.createElement("button");
		delBtn.className = "btn-sm";
		delBtn.textContent = "Delete";
		delBtn.addEventListener("click", async function() {
			if (!confirm('Delete preset "' + n + '"?')) { return; }
			try {
				const r = await fetch("/presets?name=" + encodeURIComponent(n), { method: "DELETE" });
				if (!r.ok) { throw new Error(); }
				await loadPresets();
				setMsg("Delete successful", "ok");
			} catch (e) {
				setMsg("Delete failed", "err");
			}
		});

		actions.appendChild(sendUserBtn);
		actions.appendChild(sendPassBtn);
		actions.appendChild(delBtn);

		vals.appendChild(userEl);
		vals.appendChild(passEl);
		vals.appendChild(actions);

		row.appendChild(nameEl);
		row.appendChild(vals);

		plist.appendChild(row);
	}
}

async function loadPresets() {
	try {
		const r = await fetch("/presets");
		if (!r.ok) { throw new Error(); }
		const data = await r.json();
		renderList(data);
	} catch (e) {
		setMsg("Failed to load presets", "err");
	}
}

addBtn.addEventListener("click", async function() {
	const n = pname.value.trim();
	const u = puser.value;
	const p = ppass.value;

	if (!n) {
		setMsg("Name is required", "warn");
		return;
	}

	addBtn.disabled = true;

	try {
		const body =
			"name=" + encodeURIComponent(n) +
			"&user=" + encodeURIComponent(u) +
			"&pass=" + encodeURIComponent(p);

		const r = await fetch("/presets", {
			method: "POST",
			headers: { "Content-Type": "application/x-www-form-urlencoded" },
			body
		});

		if (!r.ok) { throw new Error(); }

		pname.value = "";
		puser.value = "";
		ppass.value = "";

		await loadPresets();
		setMsg("Saving successful", "ok");
	} catch (e) {
		setMsg("Saving failed", "err");
	} finally {
		addBtn.disabled = false;
	}
});

loadPresets();
</script>
</body>
</html>
)HTML";

// ---------- HTTP Handlers ----------

// GET /
void handleRoot()
{
	server.send(200, "text/html; charset=utf-8", PAGE_HTML);
}

// POST /type  (form: text=..., newline=0|1|true|on)
void handleType()
{
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
	// Return raw JSON; front-end handles legacy upgrade view
	const String json = loadPresetsJson();
	server.send(200, "application/json; charset=utf-8", json);
}

// POST /presets (form: name, user, pass)
void handlePostPreset()
{
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

// ---------- WiFi ----------
bool connectWiFi()
{
	LogSerial.printf("[WiFi] Connecting to \"%s\"...", WIFI_SSID);

	WiFi.mode(WIFI_MODE_STA);
	WiFi.setHostname(HOSTNAME);
	WiFi.begin(WIFI_SSID, WIFI_PASS);

	const uint32_t start = millis();

	while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000U)
	{
		delay(250);
		LogSerial.print('.');
	}

	LogSerial.print("\r\n");

	if (WiFi.status() == WL_CONNECTED)
	{
		LogSerial.printf("[WiFi] OK: %s\r\n", WiFi.localIP().toString().c_str());
		return true;
	}

	LogSerial.print("[WiFi] FAILED (timeout).\r\n");
	return false;
}

// ---------- Setup / Loop ----------
void setup()
{
	// UART for logging over an external USB-UART adapter (not CDC)
	LogSerial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
	delay(100);
	LogSerial.print("\r\n=== Web Keyboard Server ===\r\n");

	USB.begin();
	Serial.begin(115200);
	delay(200);
	Keyboard.begin();
	LogSerial.print("[USB] HID Keyboard ready\r\n");
	delay(200);

	(void)connectWiFi();

	// Initialize NVS key if absent
	prefs.begin(PREFS_NS, false);
	if (!prefs.isKey(PRESETS_K))
	{
		prefs.putString(PRESETS_K, "{}");
		LogSerial.println("[NVS] Initialized empty presets");
	}
	prefs.end();

	// HTTP routes
	server.on("/", HTTP_GET, handleRoot);
	server.on("/type", HTTP_POST, handleType);
	server.on("/presets", HTTP_GET, handleGetPresets);
	server.on("/presets", HTTP_POST, handlePostPreset);
	server.on("/presets", HTTP_DELETE, handleDeletePreset);

	server.on("/favicon.ico", HTTP_GET, []()
	{
		server.send(204);
	});

	server.begin();
	LogSerial.print("[HTTP] Server started on port 80\r\n");
}

void loop()
{
	server.handleClient();
}