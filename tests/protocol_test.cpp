#include "../firmware/RemoteNode/Protocol.h"
#include <assert.h>
#include <stdio.h>
using namespace linkproto;
int main() {
  const uint8_t v[] = "123456789";
  assert(crc16(v, 9) == 0x29b1);
  assert(crc32(v, 9) == 0xcbf43926);
  Frame f;
  f.type = IMAGE;
  f.id = 0x12345678;
  f.seq = 2;
  f.total = 3;
  f.len = CHUNK;
  for (int i = 0; i < CHUNK; i++)
    f.data[i] = i;
  uint8_t raw[MAX_FRAME];
  size_t n = encode(f, raw);
  assert(n == MAX_FRAME);
  Frame out;
  assert(decode(raw, n, out));
  assert(out.id == f.id && out.seq == 2 && !memcmp(out.data, f.data, CHUNK));
  raw[20] ^= 1;
  assert(!decode(raw, n, out));
  raw[20] ^= 1;
  assert(!decode(raw, n - 1, out));
  raw[12] = 255;
  assert(!decode(raw, n, out));
  uint8_t bytes[MAX_IMAGE] = {}, source[480];
  for (int i = 0; i < 480; i++)
    source[i] = i * 7;
  Assembler image(bytes);
  assert(!image.reset(0));
  assert(!image.reset(MAX_CHUNKS + 1));
  assert(image.reset(3));
  // Lost metadata and middle chunk: record out-of-order data and request exactly what is absent.
  assert(image.add(2, source + 320, 160));
  assert(image.add(0, source, 160));
  assert(image.add(0, source, 160));
  assert(image.received == 2);
  uint8_t ack[CHUNK];
  auto size = image.ack(ack);
  assert(size == 2 && ack[0] == 1 && ack[1] == 2);
  uint8_t meta[10];
  put32(meta, 480);
  put32(meta + 4, crc32(source, 480));
  put16(meta + 8, CHUNK);
  assert(image.meta(meta, 10));
  assert(image.received == 2);
  assert(image.add(1, source + 160, 160));
  assert(image.complete());
  assert(image.ack(ack) == 2 && ack[0] == 0 && ack[1] == 0);
  assert(image.meta(meta, 10));
  assert(image.received == 3); // Duplicate metadata must not erase progress.
  bytes[5] ^= 1;
  assert(!image.complete());
  image.ack(ack);
  assert((ack[0] & 2) && ack[1] == 7 && image.received == 0);
  // Short final chunk and boundary validation.
  assert(image.reset(2));
  put32(meta, 173);
  put32(meta + 4, crc32(source, 173));
  assert(image.meta(meta, 10));
  assert(!image.add(1, source + 160, 14));
  assert(image.add(0, source, 160));
  assert(image.add(1, source + 160, 13));
  assert(image.complete());
  assert(!image.add(2, source, 1));
  puts("Protocol checks passed: CRC vectors, framing, corruption, loss recovery, duplicates, image "
       "integrity and boundaries.");
}
