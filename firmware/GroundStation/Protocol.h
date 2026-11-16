#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace linkproto {
constexpr uint16_t CHUNK = 160, MAX_IMAGE = 24576, MAX_CHUNKS = (MAX_IMAGE + CHUNK - 1) / CHUNK;
constexpr uint16_t HEADER = 14, MAX_FRAME = HEADER + CHUNK + 2, BITMAP = (MAX_CHUNKS + 7) / 8;
enum Type : uint8_t {
  COMMAND = 1,
  META = 2,
  SENSOR = 3,
  IMAGE = 4,
  END = 5,
  SENSOR_ACK = 6,
  IMAGE_ACK = 7,
  ERROR = 8
};
enum Command : uint8_t { READ_SENSOR = 1, TAKE_PHOTO = 2 };
inline uint16_t get16(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t get32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline void put16(uint8_t *p, uint16_t n) {
  p[0] = n;
  p[1] = n >> 8;
}
inline void put32(uint8_t *p, uint32_t n) {
  for (int i = 0; i < 4; i++)
    p[i] = n >> (8 * i);
}
inline uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) {
    c ^= uint16_t(*p++) << 8;
    for (int i = 0; i < 8; i++)
      c = (c & 0x8000) ? uint16_t((c << 1) ^ 0x1021) : uint16_t(c << 1);
  }
  return c;
}
inline uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xffffffff;
  while (n--) {
    c ^= *p++;
    for (int i = 0; i < 8; i++)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320 : 0);
  }
  return ~c;
}
struct Frame {
  uint8_t type = 0;
  uint32_t id = 0;
  uint16_t seq = 0, total = 0, len = 0;
  uint8_t data[CHUNK] = {};
};
inline size_t encode(const Frame &f, uint8_t *out) {
  if (f.len > CHUNK || f.type < COMMAND || f.type > ERROR)
    return 0;
  out[0] = 'L';
  out[1] = 'R';
  out[2] = 1;
  out[3] = f.type;
  put32(out + 4, f.id);
  put16(out + 8, f.seq);
  put16(out + 10, f.total);
  put16(out + 12, f.len);
  memcpy(out + HEADER, f.data, f.len);
  put16(out + HEADER + f.len, crc16(out, HEADER + f.len));
  return HEADER + f.len + 2;
}
inline bool decode(const uint8_t *raw, size_t n, Frame &f) {
  if (n < HEADER + 2 || raw[0] != 'L' || raw[1] != 'R' || raw[2] != 1 || raw[3] < COMMAND ||
      raw[3] > ERROR)
    return false;
  uint16_t len = get16(raw + 12);
  if (len > CHUNK || n != size_t(HEADER + len + 2) ||
      get16(raw + HEADER + len) != crc16(raw, HEADER + len))
    return false;
  f.type = raw[3];
  f.id = get32(raw + 4);
  f.seq = get16(raw + 8);
  f.total = get16(raw + 10);
  f.len = len;
  memcpy(f.data, raw + HEADER, len);
  return true;
}
// Storage is provided by the caller so the ground station has one bounded image buffer.
struct Assembler {
  uint8_t *bytes;
  uint8_t seen[BITMAP] = {};
  uint16_t total = 0, received = 0, lastLen = 0;
  uint32_t size = 0, checksum = 0;
  bool metadata = false;
  explicit Assembler(uint8_t *storage) : bytes(storage) {}
  bool reset(uint16_t count) {
    if (!count || count > MAX_CHUNKS)
      return false;
    total = count;
    received = lastLen = 0;
    size = checksum = 0;
    metadata = false;
    memset(seen, 0, sizeof(seen));
    return true;
  }
  bool has(uint16_t seq) const {
    return seq < total && (seen[seq / 8] & (1u << (seq % 8)));
  }
  bool meta(const uint8_t *p, uint16_t len) {
    if (len != 10 || get16(p + 8) != CHUNK)
      return false;
    uint32_t n = get32(p);
    if (!n || n > MAX_IMAGE || (n + CHUNK - 1) / CHUNK != total)
      return false;
    if (metadata)
      return n == size && get32(p + 4) == checksum;
    size = n;
    checksum = get32(p + 4);
    metadata = true;
    if (has(total - 1) && lastLen != size - uint32_t(total - 1) * CHUNK) {
      seen[(total - 1) / 8] &= ~(1u << ((total - 1) % 8));
      received--;
    }
    return true;
  }
  bool add(uint16_t seq, const uint8_t *p, uint16_t len) {
    if (seq >= total || !len || len > CHUNK || uint32_t(seq) * CHUNK + len > MAX_IMAGE)
      return false;
    uint16_t expected = seq == total - 1 ? (metadata ? size - uint32_t(seq) * CHUNK : len) : CHUNK;
    if (len != expected)
      return false;
    if (has(seq))
      return true;
    memcpy(bytes + uint32_t(seq) * CHUNK, p, len);
    seen[seq / 8] |= 1u << (seq % 8);
    received++;
    if (seq == total - 1)
      lastLen = len;
    return true;
  }
  bool complete() const {
    return metadata && received == total && crc32(bytes, size) == checksum;
  }
  // ACK flags: bit0 asks for metadata, bit1 asks for the complete image again.
  uint16_t ack(uint8_t *out) {
    uint8_t flags = metadata ? 0 : 1;
    if (metadata && received == total && !complete()) {
      flags |= 2;
      memset(seen, 0, sizeof(seen));
      received = 0;
    }
    out[0] = flags;
    uint16_t n = (total + 7) / 8;
    memset(out + 1, 0, n);
    for (uint16_t i = 0; i < total; i++)
      if (!has(i))
        out[1 + i / 8] |= 1u << (i % 8);
    return n + 1;
  }
};
} // namespace linkproto
