# Arduino IDE and dashboard setup

## 1. Install boards in Arduino IDE

Use Arduino IDE 2.x. Open **File → Preferences → Additional Boards Manager URLs**, and add these on separate lines:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Open **Tools → Board → Boards Manager** and install:

- **esp32 by Espressif Systems**, version **2.0.17** as the reference API version.
- **esp8266 by ESP8266 Community**, version **3.1.2**.

The camera sketch targets the ESP32 Arduino 2.x camera API. If upgrading to another major version, check camera structure field names and compile both sketches again.

Official instructions:
- https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
- https://arduino-esp8266.readthedocs.io/en/latest/installing.html

## 2. Install libraries

In **Sketch → Include Library → Manage Libraries**, install:

| Library | Author | Reference version |
| --- | --- | --- |
| LoRa | Sandeep Mistry | 0.8.0 |
| DHT sensor library | Adafruit | 1.4.6 |
| Adafruit Unified Sensor | Adafruit | 1.1.15 |

SPI, ESP8266WiFi and ESP32 camera support come with the board packages. Do not install similarly named radio libraries with different APIs. The ground sketch does not need DHT libraries, but installing them globally is harmless.

## 3. Upload remote firmware

1. Open `firmware/RemoteNode/RemoteNode.ino`. Keep `Protocol.h`, `RadioConfig.h` and `RadioLink.h` in the same folder; Arduino shows them as tabs.
2. Select **Tools → Board → esp32 → AI Thinker ESP32-CAM**.
3. Select the programming serial port. Use a supported camera programming carrier or a USB-to-UART programmer with 3.3 V signal levels. This is only for flashing the remote camera; the ground ESP8266 uses its own micro-USB connector.
4. Wire the camera, sensor and radio according to [WIRING.md](WIRING.md). **Disconnect DHT DATA from U0R/GPIO3 during upload.**
5. Use a stable camera supply. For a bare board programmer, connect UART TX to U0R and RX to U0T, with common ground; connect GPIO0 to ground for download mode and reset when required. Do not combine independent 5 V sources.
6. Click **Verify**, then **Upload**.
7. Remove the GPIO0-ground link, reconnect DHT DATA, then reset the assembly.
8. Remote Serial Monitor at 115200 baud should show `Remote node ready`. It may require a power-cycle if the radio was left in an unexpected state.

Default radio mapping uses GPIO12/13/14/15, with radio RESET pulled high and no DIO0 interrupt. The camera’s PSRAM and flash LED are not repurposed for radio signals. No SD card is used.

## 4. Upload ground firmware

1. Connect the NodeMCU directly to the laptop with a micro-USB data cable.
2. Open `firmware/GroundStation/GroundStation.ino`.
3. Select **Tools → Board → esp8266 → NodeMCU 1.0 (ESP-12E Module)**.
4. Select its COM/serial port; click **Verify**, then **Upload**.
5. Serial Monitor at 115200 baud should show a JSON `Ground station ready` event.
6. With **Newline** selected, send `SENSOR` to request readings or `PHOTO` to request an image. The image event is a long JSON line with Base64 data.
7. **Close Serial Monitor** before connecting the dashboard. Only one application should open the serial port.

If a COM port does not appear, check cable data support and the NodeMCU's USB-bridge driver (the exact bridge varies by board).

## 5. Set radio parameters

Both `RadioConfig.h` files must match in frequency, bandwidth, spreading factor, coding rate and sync word. The default is a 433 MHz-class Ra-02 link; confirm your module, antennas and local permitted operation. Match the radio/antenna hardware and local radio-use requirements.

The 160-byte chunk size and 24 KiB JPEG cap are defined in both `Protocol.h` files. Change both copies together and retest memory limits before increasing them.

## 6. Start Python dashboard on Windows

Install Python 3.10 or later. Open the extracted project's `dashboard` folder in File Explorer; type `cmd` into its address bar and press Enter.

```bat
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
.venv\Scripts\python.exe app.py
```

This uses the virtual environment directly without changing PowerShell execution policy. Open:

```text
http://127.0.0.1:5000
```

If port 5000 is occupied:

```bat
.venv\Scripts\python.exe app.py --port 5050
```

Then open `http://127.0.0.1:5050`.

macOS/Linux equivalent:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python app.py
```

## 7. Operate

1. Power both nodes and attach both antennas before transmitting.
2. Click **Refresh**, choose the ESP8266 serial port, then **Connect**.
3. Click **Send value**. A valid DHT reading appears on the cards and graphs.
4. Click **Send picture**. Wait for packet reception, any repair round, image reconstruction and CRC verification.
5. Download the received image if desired. Image and graph history are held in memory; restarting Python clears them.
6. Click **Disconnect** before opening Arduino Serial Monitor or uploading again.

Do not unplug the ground board during a transfer. The dashboard binds only to `127.0.0.1`; it is intended for local use.

## Troubleshooting

| Problem | Check |
| --- | --- |
| Ra-02 initialization failed | SPI pin mapping, NSS/reset, 3.3 V regulator and common local ground |
| No remote response | Same radio settings, remote power, antennas, range and interference |
| Camera cannot boot | Stable 5 V input, GPIO12 boot level, radio NSS pull-up and camera ribbon |
| Upload to ESP32-CAM fails | DHT disconnected from GPIO3, GPIO0 boot mode, programmer logic voltage |
| Sensor read fails | DHT11 wiring, pull-up, compatible supply and at least 2 seconds between physical readings |
| Port busy | Close Arduino Serial Monitor and any other serial application |
| Image exceeds 24 KiB | Increase numeric JPEG quality setting (more compression), lower resolution, or change limits with memory review |
| Many missing packets | Shorten wiring, improve regulator/antenna, reduce interference, try larger packet gaps or slower settings on both nodes |
| Transfer reaches retry limit | Fix link quality, then start a new request; code deliberately bounds recovery |
| Webpage opens but no data | Connect the correct serial port and use the command buttons |

Increasing spreading factor increases airtime and may require longer waits. Transfer duration varies with image size, radio settings and link conditions.
