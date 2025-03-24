#define DEBUG

#include "utils.h" // TODO: remove this

#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#include <cstdio>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <sys/_endian.h>
SerialClass Serial;
#else
#include <Arduino.h> // TODO: remove this
#define debug(...)
#endif

const u32 CHUNK_SIZE = 200;
const u32 IMAGE_WIDTH = 0x12345678;
const u32 IMAGE_HEIGHT = 0x1A2B3C4D;

void transmit_header(u32 n_bytes_to_send, u32 image_width, u32 image_height) {
  debug("[DEBUG] transmiting header...\n");
  u8 buffer[36];
  size_t buffer_i = 0;

  const char *prefix = "AT+TEST=TXLRPKT, \"LORA";
  memcpy(buffer + buffer_i, prefix, strlen(prefix));
  buffer_i += strlen(prefix);

  n_bytes_to_send = htonl(n_bytes_to_send);
  memcpy(buffer + buffer_i, &n_bytes_to_send, sizeof(n_bytes_to_send));
  buffer_i += sizeof(n_bytes_to_send);

  image_width = htonl(image_width);
  memcpy(buffer + buffer_i, &image_width, sizeof(image_width));
  buffer_i += sizeof(image_width);

  image_height = htonl(image_height);
  memcpy(buffer + buffer_i, &image_height, sizeof(image_height));
  buffer_i += sizeof(image_height);

  const char *suffex = "\"\n";
  memcpy(buffer + buffer_i, suffex, strlen(suffex));
  buffer_i += strlen(suffex);

  Serial.write(buffer, buffer_i);

#ifdef DEBUG
  for (size_t i = 0; i < buffer_i; ++i)
    debug("==> [%02lu] %c\t0x%02X\n", i, buffer[i], buffer[i]);
#endif

  // NOTE: Adham transmitts the body as hex and UTF-8 encodes the whole buffer
  // before sending it. I don't think i need to do this. but it is better to do
  // integration testing with the server code.
}

void transmit_image_chunk(u32 chunk_sequence_number, u8 image_bytes[],
                          u32 chunk_len) {
  // NOTE: chunk_sequence_number is now u32 (int) as opposed to u16 (short) in
  // Adham's implementation
  debug("[DEBUG] transmiting image chunk #%d (%d bytes)...\n",
        chunk_sequence_number, chunk_len);

  u8 buffer[4 + CHUNK_SIZE];
  size_t buffer_i = 0;

  // TODO: give `seq` a better name
  u32 seq_num_big_endian = htonl(chunk_sequence_number);
  memcpy(buffer + buffer_i, &seq_num_big_endian, sizeof(chunk_sequence_number));
  buffer_i += sizeof(chunk_sequence_number);

  memcpy(buffer + buffer_i, image_bytes + chunk_sequence_number * chunk_len,
         chunk_len);
  buffer_i += chunk_len;

  Serial.write(buffer, buffer_i);
}

void transmit_image_chunks(u8 image_bytes[], u32 n_chunks) {
  // transmit all chunks except the last one since it might have fewer bytes
  // than the other chunks
  for (u32 chunk_i = 0; chunk_i < n_chunks - 1; ++chunk_i)
    transmit_image_chunk(chunk_i, image_bytes, CHUNK_SIZE);
  // transmit the last chunk
  transmit_image_chunk(n_chunks - 1, image_bytes,
                       n_image_bytes - (n_chunks - 1) * CHUNK_SIZE);
}

#ifdef DEBUG
int main() {
#else
void loop() {}
void setup() {
#endif
  Serial.begin(115200);

  // manually implement the ceil function
  u32 n_chunks = n_image_bytes / CHUNK_SIZE;
  if (n_image_bytes != n_chunks * CHUNK_SIZE) {
    ++n_chunks;
  }

  u32 n_bytes_to_send = n_image_bytes + 2 * n_chunks;

  // TODO: Adham send the header 3 times. Should I do that as well?
  transmit_header(n_bytes_to_send, IMAGE_WIDTH, IMAGE_HEIGHT);
  transmit_image_chunks(image_bytes, n_chunks);

#ifdef DEBUG
  return 0;
#endif
}
