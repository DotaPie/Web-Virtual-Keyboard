# Web Virtual Keyboard
- A tool that turns a [LILYGO ESP32 T-Dongle S3](https://lilygo.cc/products/t-dongle-s3) (or any ESP32-S3 with native USB) into a Wi-Fi-controlled USB keyboard.
- The device appears to the target machine as a normal USB HID keyboard, and you drive it from any web browser on your network.
- Type free-form text, or save reusable **presets** (with optional special-key combos like `<CTRL><ALT><DEL>`) and send them with a single click.
- My use-case is to input long usernames and passwords without typing them manually, or to send CLI / BIOS / LUKS input where SSH is not possible (different networks, pre-boot environments, ...).
- Works with an ENG (US) keyboard layout on the target.

**IMPORTANT - security**
- **All text is sent as PLAINTEXT.** The web UI is protected only by HTTP Basic authentication (`MASTER_USER` / `MASTER_PASS`).
- **Use this device only on trusted local networks.**
- Preset values marked *Hidden* are never sent back to the browser - they are typed straight from the device - but they are still stored in plaintext in the device's flash.

# Features
- **Free-text typing** - type anything into a textarea and send it. `Enter` sends, `Shift+Enter` inserts a newline; an optional trailing newline can be added. Includes **📋 Paste & Send** (instant clipboard dispatch) and **👁️ Mask** (hides typed passwords on screen).
- **Presets** - save reusable snippets (name -> value) and send them with one click.
    - **Groups** - organise presets into collapsible groups. A name must be unique within a group but may repeat across different groups.
    - **Per-preset visibility**:
        - *Plaintext* - value is shown in the UI.
        - *Masked* - value is shown as `••••`.
        - *Hidden* - value never leaves the device; it is typed straight from flash and never sent to the browser.
    - **Special keys / chords** - preset values can embed `<TOKEN>` keys that plain text can't produce (see below), as well as inline delay tokens like `<DELAY 500>`. A visual *special key builder* in the editor inserts them for you.
- **MicroSD Card Storage & Payloads** - supports FAT32 microSD cards inserted into the T-Dongle S3 slot.
    - **REST File Manager** - list, view, edit, download, upload, and delete files wirelessly via the browser.
    - **DuckyScript / Macro Payload Runner** - execute `.txt` or `.ducky` payloads directly over USB HID keystrokes (supports `DELAY`, `STRING`, `ENTER`, `GUI r`, `CTRL ALT DEL`, `REPEAT`, etc.).
- **Multi-Device Target Switcher** - switch between multiple dongles from the web header and manage custom target URLs.
- **On-device status** - the display (and UART log) show the device IP once Wi-Fi connects; the web UI footer shows the firmware and storage-layout versions (from `/info`).

# Special keys (tokens)
Inside a **preset value**, wrap a key name in `< >` to send a raw key instead of literal text. Token names are case-insensitive.

Special keys use a **chord model**: each `<TOKEN>` is pressed and held as it is read, and the next typed character (if any) is pressed together with all held keys, then the whole chord is released at once. A token with no following character is pressed and released on its own.

| Preset value | Result |
| --- | --- |
| `<CTRL>c` | Ctrl + C |
| `<CTRL><ALT><DEL>` | Ctrl + Alt + Del (pressed together) |
| `<WIN>r` | Win + R (Run dialog) |
| `<F5>` | F5 on its own |
| `password<ENTER>` | types `password`, then Enter |

Supported tokens:
- **Modifiers:** `CTRL` / `CONTROL`, `SHIFT`, `ALT`, `WIN` / `GUI` / `WINDOWS` / `META`
- **Editing / whitespace:** `ENTER` / `RETURN`, `ESC` / `ESCAPE`, `BKSP` / `BACKSPACE` / `BS`, `TAB`, `SPACE` / `SPC`
- **Locks / system:** `CAPS` / `CAPSLOCK`, `PRTSC` / `PRINTSCREEN` / `PRTSCR`, `SCRLK` / `SCROLLLOCK`, `PAUSE` / `BREAK`, `NUMLK` / `NUMLOCK`, `MENU` / `APP` / `APPLICATION`
- **Navigation:** `INS` / `INSERT`, `HOME`, `PGUP` / `PAGEUP`, `DEL` / `DELETE`, `END`, `PGDN` / `PAGEDOWN`, `LEFT`, `RIGHT`, `UP`, `DOWN`
- **Function keys:** `F1`-`F12`

> Tokens are only parsed for **presets**. The quick *Type text* box sends everything literally, so `<CTRL>` there types the characters `<CTRL>`.

# Hardware requirements
- [LILYGO ESP32 T-Dongle S3](https://lilygo.cc/products/t-dongle-s3), or any other ESP32-S3 with native USB
- A web browser to access the GUI
- A target machine that will receive the keystrokes

# Quick start
- Download and open the PlatformIO project in VS Code (PlatformIO IDE extension).
- Copy `include/config.example.h` to `include/config.h` (ignored by git to keep secrets safe):
    - Set `WIFI_SSID`, `WIFI_PASS`, `MASTER_USER` and `MASTER_PASS`.
    - If using a board other than the [LILYGO ESP32 T-Dongle S3](https://lilygo.cc/products/t-dongle-s3), adjust the other board-specific values in `config.h` / `platformio.ini` as needed.
- Compile & flash.
    - `html.cpp` and `html.h` are generated automatically during the build (from `web/index.html`).
- Plug the native USB port into the target machine (and UART into another machine if you want to see the IP or logs).
    - On a successful Wi-Fi connection, the device IP is shown on the display and printed over UART.
- Open `http://DEVICE_IP` in a browser (default port is `80`), log in, and start typing / managing presets.

# Notes
- Presets are stored in the ESP32's flash (NVS). The firmware tracks a storage-layout version (`STORAGE_VERSION`); if it opens flash written by a different layout, the preset store is wiped and reinitialised - so a firmware upgrade that bumps that version will clear existing presets.

<img width="1913" height="901" alt="Snímka obrazovky 2026-07-18 193737" src="https://github.com/user-attachments/assets/b00a74c1-281e-4d7b-8bfa-7963000330d7" />
<img width="1912" height="905" alt="Snímka obrazovky 2026-07-18 193820" src="https://github.com/user-attachments/assets/b4d9b5ee-2c45-49b2-99a7-318fe79e4955" />
<img width="2576" height="1932" alt="20260719_023159" src="https://github.com/user-attachments/assets/98dcf398-3484-4ea2-9967-dc914521dc36" />



