// #define ARDUINO
#define DEBUG

#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...)
#endif // DEBUG

#ifdef ARDUINO
#include <Arduino.h> // TODO: try to remove this
#else
#include "gpt-code.hpp"
#endif // ARDUINO ---

// TODO: replace with real image
#define IMAGE_WIDTH 3
#define IMAGE_HEIGHT 2
#define N_IMAGE_BYTES 764
const u8 image_data[N_IMAGE_BYTES] = {FAKE_IMAGE};

#define CHUNK_SIZE 200

void configLora() {
  debug(">[DEBUG] sending lora configs...\n");

#ifdef DEBUG
  Serial.write("AT+LOG=DEBUG\n", 13);
#else
  Serial.write("AT+LOG=QUIET\n", 13);
#endif
  Serial.write("AT+UART=BR, 230400\n", 19);
  Serial.write("AT+MODE=TEST\n", 13);
  Serial.write("AT+TEST=RFCFG,868,SF7,250,12,15,14,ON,OFF,OFF\n", 46);

  debug("<[DEBUG] done sending lora configs\n");
}

// writes `AT+TEST=TXLRPKT, "<DATA>"\n` to serial as bytes
void sendPacket(const u8 *data, usize len) {
  Serial.write("AT+TEST=TXLRPKT, \"", 18);
  Serial.write(data, len);
  Serial.write("\"\n", 2);
  // NOTE: Adham transmitts the body as hex and UTF-8 encodes the whole buffer
  // before sending it. I don't think i need to do this. but it is better to do
  // integration testing with the server code.
}

// sends `LORA<N_BYTES_TO_SEND><IMAGE_WIDTH><IMAGE_HEIGHT>` where
// LORA is 4 bytes
// <N_BYTES_TO_SEND> is 4 bytes
// <IMAGE_WIDTH> is 4 bytes
// <IMAGE_HEIGHT> is 4 bytes
void transmitHeader(u32 n_bytes_to_send) {
  debug(">[DEBUG] transmiting header...\n");

  u8 header[16];
  void *cursor = header;

  // write "LORA" literal to buffer
  const char *prefix = "LORA";
  cursor = mempcpy(cursor, prefix, strlen(prefix));

  // write <N_BYTES_TO_SEND> to buffer
  n_bytes_to_send = htonl(n_bytes_to_send);
  cursor = mempcpy(cursor, &n_bytes_to_send, sizeof(n_bytes_to_send));

  // write <IMAGE_WIDTH> to buffer
  u32 image_width = htonl(IMAGE_WIDTH);
  cursor = mempcpy(cursor, &image_width, sizeof(image_width));

  // write <IMAGE_HEIGHT> to buffer
  u32 image_height = htonl(IMAGE_HEIGHT);
  cursor = mempcpy(cursor, &image_height, sizeof(image_height));

  sendPacket(header, 16);

  debug("<[DEBUG] transmiting header\n");
}

// sends `<CHUNK_SEQ_NUM><CHUNK_BYTES>`
void transmitImageChunks(u16 n_chunks) {
  debug(">[DEBUG] transmiting image chunks...\n");

  u8 chunk[CHUNK_SIZE + 2];
  void *cursor;
  for (u16 chunk_i = 0; chunk_i < n_chunks; ++chunk_i) {
    cursor = chunk;

    // write <CHUNK_SEQ_NUM> to buffer
    u16 chunk_seq_num = htons(chunk_i);
    cursor = mempcpy(chunk, &chunk_seq_num, sizeof(chunk_seq_num));

    // write <CHUNK_BYTES> to buffer
    usize start = chunk_i * CHUNK_SIZE;
    // last chunk might be slightly shorter than CHUNK_SIZE
    // N_IMAGE_BYTES - start is the lenght of the remaining chunks
    // it can be less than CHUNK_SIZE only if it's the last chunk
    usize chunk_len = min(CHUNK_SIZE, N_IMAGE_BYTES - start);
    cursor = mempcpy(cursor, image_data + start, chunk_len);

    sendPacket(chunk, chunk_len + 2);
  }

  debug("<[DEBUG] done transmiting image chunks\n");
}

void transmitImage() {
  // manually implement the ceil function
  u16 n_chunks = N_IMAGE_BYTES / CHUNK_SIZE;
  if (N_IMAGE_BYTES != n_chunks * CHUNK_SIZE) {
    ++n_chunks;
  }

  u32 n_bytes_to_send = N_IMAGE_BYTES + 2 * n_chunks;

  transmitHeader(n_bytes_to_send);
  transmitImageChunks(n_chunks);
}

void setup() {
  Serial.begin(115200); // TODO: this might need to be 230400
  configLora();
  transmitImage();
}

void loop() {
  // nothing to do here
}
