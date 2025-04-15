# LoRa Image Transmission (C/C++)

This project implements a prototype system for transmitting an image over a LoRa (Long Range) communication channel using AT commands. The program divides a raw image into fixed-size chunks, sends them via LoRa, and optionally retransmits missed chunks based on acknowledgments.

---

## Features

- Transmits binary image data using AT command interface
- Sends image metadata as a header: `width`, `height`, and total bytes (including header)
- Chunks image into fixed-size packets (default: 200 bytes)
- Retransmits only the missed chunks reported by the receiver
- Debug logging with compile-time toggle using macros
- Can run on Arduino or simulate behavior in a host environment

---

## Protocol Design

The image will be chunked into chunks of 200 bytes each (except for the last chunk which might be slightly shorter). Each chunk will be prefixed with a 2-byte sequence number. The first chunk has a sequence of `0` and the last chunk will have a sequence of `NUM_OF_CHUNKS - 1`.

### Header

Sent once before data transfer. Format (16 bytes total):

```
+--------+----------+-------------+--------------+
| "LORA" | DataSize | ImageWidth  | ImageHeight  |
+--------+----------+-------------+--------------+
|  4 B   |   4 B    |    4 B      |     4 B      |
+--------+----------+-------------+--------------+
```

> Note: `DataSize` is `NUM_OF_BYTES_IN_IMAGE + 2 * NUM_OF_CHUNKS` where `2 * NUM_OF_CHUNKS` corresponds to the sequence numbers.

### Retransmission

Once transmission is done, the LoRa module will be listening for the sequence number of missing chunks and then retransmit those chunks in the following format:

```
MISS<N><SEQ_1><SEQ_2>...<SEQ_N>
```

Where each `N`, the number of missed chunks, and `<SEQ_i>` are 2 bytes each.

---

## Debugging

Enable verbose debug output by uncommenting:

```c
#define DEBUG
```

---

## Platform Notes

- On **Arduino**, it uses the native `Serial` interface.
- On **non-Arduino** platforms, `utils.h` provides a mock `Serial` class for simulation.

To switch to Arduino, use:

```c
#define ARDUINO
```

Note: The project currently avoids including `Arduino.h` when not compiling for embedded environments.

---

## TODOs

- [ ] Replace fake image with actual image loader
- [ ] Consider refactoring retransmission buffer allocation
- [ ] Consider repeated header transmissions for reliability
- [ ] Ensure integration with receiver’s decoding logic (UTF-8 vs raw bytes)
- [ ] Port readBytes to simulate real input (e.g. from file)

---

## Build & Run

This is a simulation-compatible C++ project. For non-Arduino testing:

```bash
make

# run the following line to remove any temp files
make clean
```

Or, manually compile and run the program:

```bash
g++ main.cpp -o lora_tx
./lora_tx
```

---

## Credit

Special thanks to Adham (@aelmosalamy) for providing the initial implementation in Python. This repo is a port into C.
