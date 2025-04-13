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
#define N_IMAGE_BYTES 6
const u8 image_data[N_IMAGE_BYTES] = {0, 1, 2, 3, 4, 5};

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

// sends `LORA<N_BYTES_TO_SEND><IMAGE_WIDTH><IMAGE_HEIGHT>`
void transmitHeader(u32 n_bytes_to_send) {
  debug(">[DEBUG] transmiting header...\n");

  u8 header[16];
  void *cursor = header;

  // write `LORA` to buffer
  const char *prefix = "LORA";
  cursor = mempcpy(cursor, prefix, strlen(prefix));

  // write `n_bytes_to_send` to buffer
  n_bytes_to_send = htonl(n_bytes_to_send);
  cursor = mempcpy(cursor, &n_bytes_to_send, sizeof(n_bytes_to_send));

  // write `image_width` to buffer
  u32 image_width = htonl(IMAGE_WIDTH);
  cursor = mempcpy(cursor, &image_width, sizeof(image_width));

  // write `image_height` to buffer
  u32 image_height = htonl(IMAGE_HEIGHT);
  cursor = mempcpy(cursor, &image_height, sizeof(image_height));

  sendPacket(header, 16);

  debug("<[DEBUG] transmiting header\n");
}

void transmitImage() {
  // manually implement the ceil function
  u16 n_chunks = N_IMAGE_BYTES / CHUNK_SIZE;
  if (N_IMAGE_BYTES != n_chunks * CHUNK_SIZE) {
    ++n_chunks;
  }

  u32 n_bytes_to_send = N_IMAGE_BYTES + 2 * n_chunks;

  transmitHeader(n_bytes_to_send);
}

void setup() {
  Serial.begin(115200); // TODO: this might need to be 230400
  configLora();
  transmitImage();
}

void loop() {
  // nothing to do here
}
