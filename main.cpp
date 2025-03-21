// #define ARDUINO

#include <cstdio>
#include <cstring>
#ifdef ARDUINO
#include <Arduino.h>
#else
#define debug(...) fprintf(stderr, __VA_ARGS__)
#include <stdio.h>
#include <string.h>
#endif

#include "utils.h"

// ! TODO: make sure CHUNK_SIZE, n_chunks, n_bytes_to_send, n_image_bytes have
// good types keep in mind that n_chunks needs to be u16 since chunk seq is 2
// bytes (a short) in our protocol. Unless we aim to change the protocol which
// is very doable.

const u32 CHUNK_SIZE = 200;
const u32 IMAGE_WIDTH = 16909060;
const u32 IMAGE_HEIGHT = 203569230;
#define AT_COMMAND "AT+TEST=TXLRPKT, "

SerialClass Serial;

void transmit_header(u32 n_bytes_to_send, u32 image_width, u32 image_height) {
  debug("[DEBUG] transmiting header...\n");
  u8 buffer[33];
  memcpy(buffer, AT_COMMAND, 17);
  // NOTE: Adham transmitts the body as hex and encodes the whole buffer before
  // sending it. I don't think i need to do this. but it is better to do
  // integration testing with the server code. Adham also tarminates the buffer
  // with '\n'. Also double check if this is needed or maybe Serial.write does
  // that for me.
  memcpy(buffer + 17, "LORA", 4);
  write_u32(buffer + 21, n_bytes_to_send);
  write_u32(buffer + 25, image_width);
  write_u32(buffer + 29, image_height);
  Serial.write(buffer, 33);
}

void transmit_image_chunk(u16 chunk_sequence_number, u8 *image_bytes,
                          u32 chunk_len) {
  debug("[DEBUG] transmiting image #%d (%d bytes)...\n", chunk_sequence_number,
        chunk_len);
  u8 buffer[2 + CHUNK_SIZE];
  write_u16(buffer, chunk_sequence_number);
  memcpy(buffer + 2, image_bytes + chunk_sequence_number * chunk_len,
         chunk_len);
  Serial.write(buffer, chunk_len);
}

void transmit_image_chunks(u8 image_bytes[], u16 n_chunks) {
  // transmit all chunks except the last one since it might have fewer bytes
  // than the other chunks
  for (u16 chunk_i = 0; chunk_i < n_chunks - 1; ++chunk_i) {
    transmit_image_chunk(chunk_i, image_bytes, CHUNK_SIZE);
  }
  // transmit the last chunk
  transmit_image_chunk(n_chunks - 1, image_bytes,
                       n_image_bytes - (n_chunks - 1) * CHUNK_SIZE);
}

int main() {
  // manually implement the ceil function
  u32 n_chunks = n_image_bytes / CHUNK_SIZE;
  if (n_image_bytes != n_chunks * CHUNK_SIZE) {
    ++n_chunks;
  }

  u32 n_bytes_to_send = n_image_bytes + 2 * n_chunks;

  transmit_header(n_bytes_to_send, IMAGE_WIDTH, IMAGE_HEIGHT);
  transmit_image_chunks(image_bytes, n_chunks);

  return 0;
}
