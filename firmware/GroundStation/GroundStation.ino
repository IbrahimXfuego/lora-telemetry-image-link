#include "RadioLink.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
uint8_t imageBuffer[MAX_IMAGE];
Assembler image(imageBuffer);
uint32_t transaction = 0, finishedId = 0;
uint8_t requested = 0, finishedType = 0;
uint16_t finishedTotal = 0;
bool pending = false, responded = false;
unsigned long lastActivity = 0, lastCommand = 0;
int commandAttempts = 0;
char input[48];
size_t inputLength = 0;
void event(const char *message) {
  Serial.print("{\"type\":\"status\",\"message\":\"");
  Serial.print(message);
  Serial.println("\"}");
}
void error(const char *message) {
  Serial.print("{\"type\":\"error\",\"message\":\"");
  for (const char *p = message; *p; p++) {
    if (*p == '"' || *p == '\\')
      Serial.print('\\');
    if (uint8_t(*p) >= 32)
      Serial.print(*p);
  }
  Serial.println("\"}");
}
void sendCommand() {
  Frame f;
  f.type = COMMAND;
  f.id = transaction;
  f.len = 1;
  f.data[0] = requested;
  transmit(f);
  lastCommand = millis();
  commandAttempts++;
}
void request(uint8_t kind) {
  if (pending) {
    error("Transfer already in progress");
    return;
  }
  transaction++;
  if (!transaction)
    transaction++;
  requested = kind;
  pending = true;
  responded = false;
  commandAttempts = 0;
  lastActivity = millis();
  image.total = 0;
  sendCommand();
  event(kind == READ_SENSOR ? "Sensor command sent" : "Image command sent");
}
void sendSimpleAck(uint32_t id, uint8_t type, uint16_t total = 0) {
  Frame f;
  f.type = type;
  f.id = id;
  f.total = total;
  if (type == IMAGE_ACK) {
    f.len = 1 + (total + 7) / 8;
    memset(f.data, 0, f.len);
  }
  transmit(f);
}
void printImage() {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  Serial.print("{\"type\":\"image\",\"id\":");
  Serial.print(transaction);
  Serial.print(",\"crc32\":");
  Serial.print(image.checksum);
  Serial.print(",\"bytes\":");
  Serial.print(image.size);
  Serial.print(",\"data\":\"");
  for (uint32_t i = 0; i < image.size; i += 3) {
    uint32_t n = uint32_t(imageBuffer[i]) << 16;
    if (i + 1 < image.size)
      n |= uint32_t(imageBuffer[i + 1]) << 8;
    if (i + 2 < image.size)
      n |= imageBuffer[i + 2];
    Serial.write(table[(n >> 18) & 63]);
    Serial.write(table[(n >> 12) & 63]);
    Serial.write(i + 1 < image.size ? table[(n >> 6) & 63] : '=');
    Serial.write(i + 2 < image.size ? table[n & 63] : '=');
    if (i % 300 == 0)
      yield();
  }
  Serial.println("\"}");
}
void process(const Frame &f) {
  if (f.id == finishedId && finishedId) {
    if (f.type == END && finishedType == TAKE_PHOTO && f.total == finishedTotal)
      sendSimpleAck(f.id, IMAGE_ACK, f.total);
    if (f.type == SENSOR && finishedType == READ_SENSOR)
      sendSimpleAck(f.id, SENSOR_ACK);
    return;
  }
  if (!pending || f.id != transaction)
    return;
  if (f.type == ERROR) {
    char text[CHUNK + 1];
    memcpy(text, f.data, f.len);
    text[f.len] = 0;
    error(text);
    pending = false;
    return;
  }
  if (requested == READ_SENSOR && f.type == SENSOR) {
    if (f.total != 1 || !f.len)
      return;
    sendSimpleAck(f.id, SENSOR_ACK);
    char text[CHUNK + 1];
    memcpy(text, f.data, f.len);
    text[f.len] = 0;
    Serial.print("{\"type\":\"sensor\",\"rssi\":");
    Serial.print(LoRa.packetRssi());
    Serial.print(",\"values\":");
    Serial.print(text);
    Serial.println("}");
    finishedId = f.id;
    finishedType = READ_SENSOR;
    pending = false;
    return;
  }
  if (requested != TAKE_PHOTO || (f.type != META && f.type != IMAGE && f.type != END))
    return;
  if (!f.total || f.total > MAX_CHUNKS)
    return;
  if (!image.total) {
    if (!image.reset(f.total))
      return;
  }
  if (image.total != f.total)
    return;
  bool accepted = false;
  if (f.type == META)
    accepted = image.meta(f.data, f.len);
  if (f.type == IMAGE)
    accepted = image.add(f.seq, f.data, f.len);
  if (f.type == END) {
    Frame ack;
    ack.type = IMAGE_ACK;
    ack.id = f.id;
    ack.total = f.total;
    ack.len = image.ack(ack.data);
    const bool complete = image.complete();
    transmit(ack);
    accepted = true;
    if (complete) {
      printImage();
      finishedId = f.id;
      finishedType = TAKE_PHOTO;
      finishedTotal = f.total;
      pending = false;
      event("Image reconstructed and CRC32 verified");
      return;
    }
    event("End received; requested missing packets");
  }
  if (accepted) {
    responded = true;
    lastActivity = millis();
    Serial.print("{\"type\":\"progress\",\"received\":");
    Serial.print(image.received);
    Serial.print(",\"total\":");
    Serial.print(image.total);
    Serial.print(",\"rssi\":");
    Serial.print(LoRa.packetRssi());
    Serial.println("}");
  }
}
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(10);
  SPI.begin();
  if (!startRadio()) {
    error("Ra-02 initialization failed; check power and SPI");
    while (true)
      delay(1000);
  }
  transaction = ESP.getCycleCount();
  event("Ground station ready");
}
void loop() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r')
      continue;
    if (ch == '\n') {
      input[inputLength] = 0;
      if (!strcmp(input, "SENSOR"))
        request(READ_SENSOR);
      else if (!strcmp(input, "PHOTO"))
        request(TAKE_PHOTO);
      else if (!strcmp(input, "PING"))
        event("Ground station ready");
      else
        error("Use SENSOR or PHOTO");
      inputLength = 0;
    } else if (inputLength < sizeof(input) - 1)
      input[inputLength++] = ch;
  }
  Frame f;
  if (receiveFrame(f))
    process(f);
  const unsigned long now = millis();
  if (pending && !responded && now - lastCommand > 2500) {
    if (commandAttempts < 4)
      sendCommand();
    else {
      pending = false;
      error("No response; check remote node and matching radio settings");
    }
  }
  if (pending && responded && now - lastActivity > 30000) {
    pending = false;
    error("Transfer timed out; request again");
  }
  delay(1);
}
