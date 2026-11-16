#include "RadioLink.h"
#include "esp_camera.h"
#include <Arduino.h>
#include <DHT.h>
#include <SPI.h>
DHT dht(DHT_PIN, DHT11);
uint32_t lastCompleted = 0;
unsigned long lastCompletedAt = 0;
void sendError(uint32_t id, const char *message) {
  Frame f;
  f.type = ERROR;
  f.id = id;
  f.len = strlen(message);
  memcpy(f.data, message, f.len);
  transmit(f);
}
bool cameraBegin() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = 5;
  c.pin_d1 = 18;
  c.pin_d2 = 19;
  c.pin_d3 = 21;
  c.pin_d4 = 36;
  c.pin_d5 = 39;
  c.pin_d6 = 34;
  c.pin_d7 = 35;
  c.pin_xclk = 0;
  c.pin_pclk = 22;
  c.pin_vsync = 25;
  c.pin_href = 23;
  c.pin_sscb_sda = 26;
  c.pin_sscb_scl = 27;
  c.pin_pwdn = 32;
  c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_QVGA;
  c.jpeg_quality = 16;
  c.fb_count = 1;
  c.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  return esp_camera_init(&c) == ESP_OK;
}
bool waitAck(uint8_t type, uint32_t id, Frame &reply) {
  unsigned long start = millis();
  while (millis() - start < ACK_TIMEOUT_MS) {
    Frame f;
    if (receiveFrame(f) && f.id == id && f.type == type) {
      reply = f;
      return true;
    }
    delay(1);
  }
  return false;
}
void sensorResponse(uint32_t id) {
  float t = dht.readTemperature(), h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    sendError(id, "DHT11 read failed");
    return;
  }
  Frame f;
  f.type = SENSOR;
  f.id = id;
  f.total = 1;
  f.len = snprintf(reinterpret_cast<char *>(f.data), CHUNK,
                   "{\"temperature\":%.1f,\"humidity\":%.1f}", t, h);
  for (int i = 0; i < 4; i++) {
    transmit(f);
    Frame ack;
    if (waitAck(SENSOR_ACK, id, ack))
      break;
  }
}
void imageResponse(uint32_t id) {
  // Discard one buffered frame so the request gets a recent capture.
  camera_fb_t *stale = esp_camera_fb_get();
  if (stale)
    esp_camera_fb_return(stale);
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    sendError(id, "Camera capture failed");
    return;
  }
  if (!fb->len || fb->len > MAX_IMAGE) {
    esp_camera_fb_return(fb);
    sendError(id, "JPEG exceeds 24 KiB; increase jpeg_quality number or lower resolution");
    return;
  }
  const uint16_t total = (fb->len + CHUNK - 1) / CHUNK;
  Frame meta;
  meta.type = META;
  meta.id = id;
  meta.total = total;
  meta.len = 10;
  put32(meta.data, fb->len);
  put32(meta.data + 4, crc32(fb->buf, fb->len));
  put16(meta.data + 8, CHUNK);
  auto sendChunk = [&](uint16_t seq) {
    Frame f;
    f.type = IMAGE;
    f.id = id;
    f.seq = seq;
    f.total = total;
    uint32_t offset = uint32_t(seq) * CHUNK;
    f.len = uint16_t((fb->len - offset) > CHUNK ? CHUNK : fb->len - offset);
    memcpy(f.data, fb->buf + offset, f.len);
    transmit(f);
  };
  transmit(meta);
  transmit(meta);
  for (uint16_t seq = 0; seq < total; seq++)
    sendChunk(seq);
  Frame end;
  end.type = END;
  end.id = id;
  end.total = total;
  for (int round = 0; round <= MAX_REPAIR_ROUNDS; round++) {
    transmit(end);
    Frame ack;
    if (!waitAck(IMAGE_ACK, id, ack))
      continue; // END or ACK lost: repeat END, not the whole burst.
    if (ack.total != total || ack.len != 1 + (total + 7) / 8)
      continue;
    const bool needMeta = ack.data[0] & 1, needAll = ack.data[0] & 2;
    bool missing = false;
    for (uint16_t seq = 0; seq < total; seq++)
      if (ack.data[1 + seq / 8] & (1u << (seq % 8)))
        missing = true;
    if (!needMeta && !needAll && !missing) {
      esp_camera_fb_return(fb);
      return;
    }
    if (round == MAX_REPAIR_ROUNDS)
      break;
    if (needMeta)
      transmit(meta);
    for (uint16_t seq = 0; seq < total; seq++)
      if (needAll || (ack.data[1 + seq / 8] & (1u << (seq % 8))))
        sendChunk(seq);
  }
  esp_camera_fb_return(fb);
  sendError(id, "Image ACK/retry limit reached");
}
void setup() {
  Serial.begin(115200);
  delay(200);
  dht.begin();
  // RX0/GPIO3 is a DHT pin in normal operation. Disconnect DHT while uploading.
  if (!cameraBegin()) {
    Serial.println("Camera initialization failed");
    while (true)
      delay(1000);
  }
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);
  if (!startRadio()) {
    Serial.println("Ra-02 initialization failed: check 3.3 V and SPI");
    while (true)
      delay(1000);
  }
  Serial.println("Remote node ready");
  delay(2000);
}
void loop() {
  Frame command;
  if (receiveFrame(command) && command.type == COMMAND && command.len == 1) {
    if (command.id == lastCompleted && millis() - lastCompletedAt < 60000)
      return;
    if (command.data[0] == READ_SENSOR)
      sensorResponse(command.id);
    else if (command.data[0] == TAKE_PHOTO)
      imageResponse(command.id);
    else {
      sendError(command.id, "Unknown command");
      return;
    }
    lastCompleted = command.id;
    lastCompletedAt = millis();
  }
  delay(1);
}
