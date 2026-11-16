#pragma once
// Match these in both sketches. Choose frequency, power and pacing permitted locally.
constexpr long RADIO_HZ = 433000000;
constexpr long RADIO_BW = 125000;
constexpr int RADIO_SF = 8, RADIO_CR = 5, RADIO_POWER_DBM = 10;
constexpr uint8_t RADIO_SYNC = 0x34;
constexpr unsigned long PACKET_GAP_MS = 20, ACK_TIMEOUT_MS = 3500;
constexpr int MAX_REPAIR_ROUNDS = 5;
// AI-Thinker ESP32-CAM, without microSD. DIO0 is unused: radio is polled.
constexpr int RADIO_SCK = 14, RADIO_MISO = 12, RADIO_MOSI = 13, RADIO_NSS = 15;
constexpr int RADIO_RST = -1, RADIO_DIO0 = -1, DHT_PIN = 3;
