# Wiring

Follow this code-matched pin mapping. SPI signal names identify each connection; GPIO assignments must match the sketches.

## Ground: NodeMCU ESP8266 → Ra-02

| Ra-02 | NodeMCU | GPIO |
| --- | --- | --- |
| 3.3V | 3V3 regulated rail | Power, never 5 V |
| GND | GND | Common local reference |
| SCK | D5 | GPIO14 |
| MISO | D6 | GPIO12 |
| MOSI | D7 | GPIO13 |
| NSS | D1 | GPIO5 |
| RESET | D2 | GPIO4 |
| DIO0–DIO5 | Not connected | Firmware polls radio registers |
| ANT | Matched antenna | Attach before TX |

NodeMCU connects **directly to the laptop through micro-USB**. The cable carries power and USB serial data via the board's built-in USB bridge. No separate external supply or USB-to-TTL adapter is required on the ground side. Use a data-capable cable.

The NodeMCU's regulator must have adequate current headroom for the radio. If its 3.3 V rail sags during TX, improve the local regulated 3.3 V supply; do not move the radio to 5 V. Fit decoupling near each Ra-02, for example 100 nF ceramic plus 10–100 µF bulk, as a starting point for supply verification.

## Remote: AI-Thinker ESP32-CAM → Ra-02 / DHT11

| Peripheral pin | ESP32-CAM connection |
| --- | --- |
| Ra-02 SCK | GPIO14 |
| Ra-02 MISO | GPIO12 |
| Ra-02 MOSI | GPIO13 |
| Ra-02 NSS | GPIO15 |
| Ra-02 RESET | Pull up to 3.3 V with 10 kΩ; MCU reset pin unused in software |
| Ra-02 3.3V / GND | Regulated 3.3 V / local GND |
| Ra-02 DIO pins | Not connected; polling mode |
| Ra-02 ANT | Matched antenna |
| DHT11 DATA | **GPIO3 / U0R**, disconnected while uploading |
| DHT11 VCC / GND | 3.3 V / local GND, for a 3.3 V-compatible sensor/module |
| DHT11 data pull-up | 4.7–10 kΩ to 3.3 V if not already on the module |
| ESP32-CAM 5V / GND | External regulated 5 V adapter / local GND |

Important ESP32-CAM constraints:

- The camera already occupies its camera-interface pins. PSRAM uses additional pins; GPIO16 is intentionally avoided.
- No microSD card is used; several selected pins overlap its interface.
- GPIO12 is an ESP32 boot-strapping pin. Fit a 10 kΩ pull-down to GND and a 10 kΩ NSS pull-up to 3.3 V so the radio stays deselected during boot. Confirm the radio does not drive MISO high at reset.
- DHT DATA uses GPIO3 because the board has limited exposed pins. Disconnect the DHT data wire for programming, then reconnect after uploading. Serial input is not used in normal remote-node operation.
- The radio RESET pull-up defines normal operation when software reset is omitted. Power-cycle the remote assembly if the radio needs a hard reset.
- Use these pull-ups, pull-downs and local decoupling with the supplied wiring.

## Power

The remote camera board is powered by an AC/DC adapter. Use a regulated low-voltage output appropriate to the camera board, normally 5 V on its 5V input, with adequate peak-current headroom. Feed each Ra-02 only from a suitable 3.3 V regulator. Connect grounds within each local assembly. The wireless link needs no ground wire between remote and ground nodes.

Never connect a raw AC supply to any board. Do not parallel independent supply outputs. Use 3.3 V UART logic while programming the ESP32-CAM; any temporary programmer used to flash it is separate from the ground station's normal micro-USB connection.
