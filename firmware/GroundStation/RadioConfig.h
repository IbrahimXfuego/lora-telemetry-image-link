#pragma once
constexpr long RADIO_HZ = 433000000;
constexpr long RADIO_BW = 125000;
constexpr int RADIO_SF = 8, RADIO_CR = 5, RADIO_POWER_DBM = 10;
constexpr uint8_t RADIO_SYNC = 0x34;
constexpr unsigned long PACKET_GAP_MS = 20, ACK_TIMEOUT_MS = 3500;
constexpr int MAX_REPAIR_ROUNDS = 5;
// NodeMCU hardware SPI: D5 SCK, D6 MISO, D7 MOSI. DIO0 unused.
constexpr int RADIO_NSS = D1, RADIO_RST = D2, RADIO_DIO0 = -1;
