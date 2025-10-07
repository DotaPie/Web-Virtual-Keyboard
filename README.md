# Web Virtual Keyboard
- This is a tool to create virtual keyboard with ESP32-S3
- It can be accessed via web GUI and it can either input custom text, or use saved presets
- Preset are made from username and password and created/edited/deleted via web GUI
- My use-case is to input long usernames and passwords without the need of typing them manually or to input CLI commands, where SSH is not easily possible (because of different networks, ...)
- Works only with ENG keyboard on target

**IMPORTANT**
- **Usernames and passwords are sent as PLAINTEXT, device is only secured by simple login (once per session)**
- **It is strongly recommended to use this device only on secured local networks**

<img width="1227" height="503" alt="Snímka obrazovky 2025-09-25 215729" src="https://github.com/user-attachments/assets/3959b888-51c9-48d7-bdba-ff45bc24d5d8" />

# Hardware requirements
- ESP32-S3 with native USB
    - UART is optional, it is used to check ESP32-S3 IP in network and to read debug messages
- Other machine to access web GUI
- Target machine that will use this virtual keyboard

# Quick start
- Download and open platformio project in VS Code
- Change `WIFI_SSID`, `WIFI_PASS`, `MASTER_USER` and `MASTER_PASS` in `config.h`
    - If using other board than [ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html#getting-started), change also other params in `config.h` or `platformio.ini` as needed
- Compile & Run
    - `html.cpp` and `html.h` are automatically generated during the build (from `index.html`)
- If connection is successful, IP of the device is printed in UART
- Plug native USB into the target machine (and UART to the other machine, if you want to check IP or logs)
- On other machine you can then access device by `http://DEVICE_IP` (default port is 80)
