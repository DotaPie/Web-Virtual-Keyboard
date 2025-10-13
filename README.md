# Web Virtual Keyboard
- This is a tool to create virtual keyboard with [LILYGO ESP32 T-Dongle S3](https://lilygo.cc/products/t-dongle-s3)
    - works on any other ESP32-S3, requires small modification in `config.h` and `platformio.ini`
- It can be accessed via web browser and it can either input custom text, or use saved presets
    - Presets are made from username and password and created/edited/deleted via web browser too
- My use-case is to input long usernames and passwords without the need of typing them manually or to input CLI commands, where SSH is not easily possible (because of different networks, BIOS, LUKS, ...)
- Works only with ENG keyboard on target

**IMPORTANT**
- **All text is sent as PLAINTEXT, device is only secured by simple login (once per session)**
- **It is strongly recommended to use this device only on secured local networks**

# Hardware requirements
- [LILYGO ESP32 T-Dongle S3](https://lilygo.cc/products/t-dongle-s3)
    - or any other ESP32-S3 with native USB
- Web browser to access GUI
- Target machine that will use this virtual keyboard

# Quick start
- Download and open platformio project in VS Code
- Change `WIFI_SSID`, `WIFI_PASS`, `MASTER_USER` and `MASTER_PASS` in `config.h`
    - If using other board than [LILYGO ESP32 T-Dongle S3](https://lilygo.cc/products/t-dongle-s3), change also other params in `config.h` or `platformio.ini` as needed
- Compile & Flash
    - `html.cpp` and `html.h` are automatically generated during the build (from `web/index.html`)
- Plug native USB into the target machine (and UART to the other machine, if you want to check IP or logs)
    - If connection to Wi-Fi is successful, IP of the device is shown on display (and printed over UART)
- In browser you can now access device by `http://DEVICE_IP` (default port is 80)


<img width="1230" height="560" alt="pic" src="https://github.com/user-attachments/assets/e1dac6ce-4c9d-4045-87dd-af628fd6060f" />

![1000002307](https://github.com/user-attachments/assets/b35ac7f9-3ec2-4a7a-bad3-ffd8b0127b56)
