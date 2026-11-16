# Components

| Component | Quantity | Notes |
| --- | --- | --- |
| AI-Thinker ESP32-CAM | 1 | Camera included; remote sensing node |
| NodeMCU ESP8266 | 1 | Built-in USB serial bridge; ground node |
| Ra-02 LoRa module | 2 | Matching hardware/frequency, SPI interface |
| Matched radio antenna | 2 | Attach before transmission |
| DHT11 sensor/module | 1 | 3.3 V-compatible data/supply arrangement |
| Micro-USB data cable | 1 | Ground node power and serial data to laptop |
| Regulated camera adapter | 1 | Suitable 5 V output/current for ESP32-CAM input |
| Suitable 3.3 V regulation | Per assembly as needed | Radio TX current needs adequate headroom |
| Jumper wires/breadboard | As needed | Short SPI and power wiring |
| 10 kΩ resistors | 3 suggested | Remote NSS pull-up, MISO pull-down, radio reset pull-up |
| DHT pull-up | 1 if absent | 4.7–10 kΩ to 3.3 V |
| Decoupling capacitors | Per radio | Suggested 100 nF plus 10–100 µF near supply |
| Camera programming carrier/adapter | Temporary | For firmware upload only; not a separate ground-side UART adapter |

Use the suggested pull-ups and decoupling for the pin mapping in this package. Match adapter ratings to the board and radio loads. The project contains no motors, wheels, GPS or soil sensor.
