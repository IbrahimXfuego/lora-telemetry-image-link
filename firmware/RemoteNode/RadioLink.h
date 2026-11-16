#pragma once
#include "Protocol.h"
#include "RadioConfig.h"
#include <LoRa.h>
using namespace linkproto;
inline bool transmit(const Frame &f) {
  uint8_t bytes[MAX_FRAME];
  size_t n = encode(f, bytes);
  if (!n)
    return false;
  if (!LoRa.beginPacket())
    return false;
  LoRa.write(bytes, n);
  int ok = LoRa.endPacket();
  LoRa.receive();
  delay(PACKET_GAP_MS);
  return ok == 1;
}
inline bool receiveFrame(Frame &f) {
  int n = LoRa.parsePacket();
  if (!n)
    return false;
  uint8_t bytes[MAX_FRAME];
  if (n > int(sizeof(bytes))) {
    while (LoRa.available())
      LoRa.read();
    return false;
  }
  int read = 0;
  while (LoRa.available() && read < n)
    bytes[read++] = LoRa.read();
  return decode(bytes, read, f);
}
inline bool startRadio() {
  LoRa.setPins(RADIO_NSS, RADIO_RST, RADIO_DIO0);
  LoRa.setSPIFrequency(1000000);
  if (!LoRa.begin(RADIO_HZ))
    return false;
  LoRa.setSignalBandwidth(RADIO_BW);
  LoRa.setSpreadingFactor(RADIO_SF);
  LoRa.setCodingRate4(RADIO_CR);
  LoRa.setSyncWord(RADIO_SYNC);
  LoRa.setTxPower(RADIO_POWER_DBM);
  LoRa.enableCrc();
  LoRa.receive();
  return true;
}
