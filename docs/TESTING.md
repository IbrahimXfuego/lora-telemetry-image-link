# Checks and integration

## Protocol checks

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/protocol_test.cpp -o protocol_check
./protocol_check
```

Covers CRC vectors, frame decoding, corrupt/truncated frames, lost/reordered/duplicate chunks, metadata handling, missing-packet reports and image boundaries.

## Dashboard checks

```sh
python -m pip install -r dashboard/requirements.txt
python -m unittest discover -s tests -p "test_*.py" -v
```

Covers sensor events, image length/CRC, API controls, packet progress, fragmented serial data and shared headers.

## Integration sequence

1. Verify and upload both Arduino sketches.
2. Confirm stable radio power and matching radio settings.
3. Request sensor data with nodes nearby and antennas attached.
4. Request an image and observe reception counts, ACK and any repair round.
5. Check repeated packets do not inflate the received count.
6. Interrupt a transfer and check timeout/recovery behaviour.
7. Record image size, settings, duration and retries when comparing link configurations.
