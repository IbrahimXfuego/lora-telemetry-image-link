# Radio protocol · version 1

## Common packet

All multibyte integers use little-endian byte order. Fields are encoded explicitly; there is no packed-struct or CPU-endianness dependency.

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 2 | ASCII `LR` |
| 2 | 1 | Protocol version `1` |
| 3 | 1 | Type |
| 4 | 4 | Transaction ID |
| 8 | 2 | Sequence index, zero-based |
| 10 | 2 | Total image chunks, or 1 for sensor response |
| 12 | 2 | Payload length |
| 14 | 0–160 | Payload |
| 14 + length | 2 | CRC16 of header plus payload |

Maximum frame size: 176 bytes, below the radio library's 255-byte payload limit. LoRa hardware payload CRC is also enabled. Application CRC16 uses CCITT-FALSE: polynomial `0x1021`, initial `0xFFFF`, no reflection, no final XOR; `123456789` → `0x29B1`.

Image CRC32 is CRC-32/ISO-HDLC, reflected polynomial `0xEDB88320`, initial/final XOR `0xFFFFFFFF`; `123456789` → `0xCBF43926`.

## Types

| Value | Name | Payload |
| --- | --- | --- |
| 1 | COMMAND | One byte: 1 = sensor, 2 = photo |
| 2 | META | Image bytes uint32, image CRC32 uint32, chunk bytes uint16 |
| 3 | SENSOR | UTF-8 JSON with temperature and humidity |
| 4 | IMAGE | Raw JPEG segment, at byte offset sequence × 160 |
| 5 | END | Empty; begins the feedback window |
| 6 | SENSOR_ACK | Empty |
| 7 | IMAGE_ACK | Flags byte followed by missing bitmap |
| 8 | ERROR | Short UTF-8 text |

One image is limited to 24,576 bytes, giving at most 154 chunks. Full chunks are 160 bytes; the final chunk may be shorter.

## Image acknowledgement

The ACK header repeats the transaction ID and total chunk count. Payload length must equal `1 + ceil(total / 8)`.

- Flags bit 0: metadata is missing; resend META.
- Flags bit 1: all chunks were present but whole-image CRC failed; resend the complete image.
- Following bytes: bit `sequence % 8` of byte `1 + sequence / 8` is 1 if that chunk is missing.
- Zero flags and a zero bitmap mean complete, CRC-verified reception.

The receiver can collect chunks before META because chunk size and maximum image capacity are fixed. The declared image length is applied once META arrives; malformed lengths and out-of-bounds sequences are rejected. Duplicate META does not clear already received chunks.

## Recovery

- Initial META is sent twice. All chunks follow, then END.
- The remote waits 3.5 seconds for a valid matching ACK.
- An absent ACK causes another END, without retransmitting the full burst.
- A missing bitmap causes only those chunks to be resent, followed by END.
- A metadata request causes META to be resent.
- Whole-image CRC failure clears the receiver's chunk bitmap and requests all chunks.
- Up to five repair rounds follow the initial burst. After the limit, the remote emits an error and releases the camera frame buffer.
- The ground caches the last completed transaction and re-ACKs a repeated END, without emitting the same image again.

Sensor packets are sent up to four times, waiting for SENSOR_ACK between attempts. Ground requests with no response are retried up to four total command transmissions. The ground abandons an active partial transfer after 30 seconds without an accepted packet. The Python dashboard also has a five-minute overall UI timeout.

There is one remote and one ground node. Transaction IDs correlate a request and its packets; they are not cryptographic authentication.

## USB serial protocol

Host commands are ASCII lines: `PING`, `SENSOR`, `PHOTO`, terminated by newline.

Ground events are newline-delimited JSON:

```json
{"type":"sensor","rssi":-88,"values":{"temperature":27.5,"humidity":62.0}}
{"type":"progress","received":14,"total":60,"rssi":-90}
{"type":"status","message":"End received; requested missing packets"}
```

An `image` event contains `id`, numeric `crc32`, `bytes` and Base64 `data`. Python validates the length, CRC32 and JPEG start/end markers. Ground emits the Base64 stream directly without allocating another complete encoded-image buffer. Boot messages that are not JSON are ignored by the dashboard.
