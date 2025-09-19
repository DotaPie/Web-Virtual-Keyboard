# Web Virtual Keyboard
- This is a tool to create virtual keyboard with ESP32-S3
- It can be access via web GUI and it can either input custom text, or use saved presets (which are also stored via web GUI)
- My use-case is to input long passwords without the need of typing them manually (where SSH is not possible)
- Works only with ENG keyboard on target

<img width="825" height="460" alt="Snímka obrazovky 2025-09-19 230531" src="https://github.com/user-attachments/assets/6764f580-fd7d-4e62-a432-80344b9e164b" />

# Hardware requirements
- ESP32-S3 with native USB
    - UART is optional, it is used to check ESP32-S3 IP in network and to read debug messages
- Other machine to access web GUI

# Quick start
- Download and open platformio project in VS Code
- Change `WIFI_SSID` and `WIFI_PASS`
    - If needed, change also `UART_RX_PIN` and `UART_TX_PIN`
- Compile & Run
- If connection is successful, IP of the device is printed in UART
- Plug native USB into the target machine, and UART to other machine, if you want to check IP or logs
- On other machine you can then access device by `http://DEVICE_IP`
