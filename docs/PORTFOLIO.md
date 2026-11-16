# Portfolio content

**LoRa-Based Bidirectional Telemetry and Image Transfer**

A communication prototype connecting a remote ESP32-CAM sensing node to an ESP8266 ground station through Ra-02 LoRa radios, with a Python browser dashboard.

- Sent dashboard commands over USB serial and LoRa to request DHT11 readings or camera images.
- Split JPEG images into radio packets and reconstructed them at the ground station.
- Used CRC checks, receiver acknowledgements and retransmission to handle transfer errors.
- Displayed sensor graphs, packet reception status and received images in a Python-based web interface.

The remote node represents the communication subsystem of a rover; the project did not include a moving platform. Add the collaborator's name and your specific responsibilities once confirmed.

Use actual build photos/demo recordings when available. The supplied wiring illustration can be used as a technical illustration, not as a photograph of the original build. Avoid advertising a specific range or reliability percentage without measurements.
