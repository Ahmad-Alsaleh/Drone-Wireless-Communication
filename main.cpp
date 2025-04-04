#define DEBUG

#include "utils.h" // TODO: remove this

#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_endian.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

class SerialClass {
public:
  void begin(unsigned long baud) { debug("Starting Serial (%ld)\n", baud); }
  void write(const u8 *bytes, size_t length) {
    for (size_t i = 0; i < length - 1; ++i)
      printf("%02x ", bytes[i]);
    printf("%02x\n", bytes[length - 1]);
  }
  void print(const char *msg) { printf("%s", msg); }
  void println(const char *msg) { printf("%s\n", msg); }
} Serial;

#else
#include <Arduino.h> // TODO: remove this
#define debug(...)
#endif

const u32 CHUNK_SIZE = 200; // no need for u32, maybe use #define
const u32 IMAGE_WIDTH = 10;
const u32 IMAGE_HEIGHT = 7;

// TODO: Adham sends the header as a prefix to the 1st chunk.
// However, I send it as a separate chunk to simplyfy the code.
// To fix this, make a function called `transmit_first_chunk` and
// put it inside `transmit_image_chunks`
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

  // NOTE: Adham transmitts the body as hex and UTF-8 encodes the whole buffer
  // before sending it. I don't think i need to do this. but it is better to do
  // integration testing with the server code.
}

void transmit_image_chunk(u16 chunk_seq_num, u8 image_bytes[], u32 chunk_len) {
  debug("[DEBUG] transmiting image chunk #%d (%d bytes)...\n", chunk_seq_num,
        chunk_len);

  u8 buffer[22 + CHUNK_SIZE];
  size_t buffer_i = 0;

  const char *prefix = "AT+TEST=TXLRPKT, \"";
  memcpy(buffer + buffer_i, prefix, strlen(prefix));
  buffer_i += strlen(prefix);

  u16 seq_num_big_endian = htons(chunk_seq_num);
  memcpy(buffer + buffer_i, &seq_num_big_endian, sizeof(chunk_seq_num));
  buffer_i += sizeof(chunk_seq_num);

  memcpy(buffer + buffer_i, image_bytes + chunk_seq_num * CHUNK_SIZE,
         chunk_len);
  buffer_i += chunk_len;

  const char *suffex = "\"\n";
  memcpy(buffer + buffer_i, suffex, strlen(suffex));
  buffer_i += strlen(suffex);

  Serial.write(buffer, buffer_i);
}

void transmit_image_chunks(u8 image_bytes[], u16 n_chunks) {
  // transmit all chunks except the last one since it might have fewer bytes
  // than the other chunks
  for (u16 chunk_i = 0; chunk_i < n_chunks - 1; ++chunk_i)
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
  u16 n_chunks = n_image_bytes / CHUNK_SIZE;
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
