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
#define RETRANSMISSION_TIMEOUT 5
#define RX_SWITCH_DELAY 500

void configLora() {
  debug(">[DEBUG] sending lora configs...\n");

  // TODO: read after every write to discard confirmation msg
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
  // TODO: Adham is sending the header 3 times in a row, do i need to do this?

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

void transmitSingleImageChunk(u16 chunk_seq_num) {
  debug(">[DEBUG] transmiting image chunk with seq #%hu...\n", chunk_seq_num);

  u8 chunk[CHUNK_SIZE + 2];
  void *cursor = chunk;

  // write <CHUNK_SEQ_NUM> to buffer
  chunk_seq_num = htons(chunk_seq_num);
  cursor = mempcpy(cursor, &chunk_seq_num, sizeof(chunk_seq_num));

  // write <CHUNK_BYTES> to buffer
  usize start = chunk_seq_num * CHUNK_SIZE;
  // last chunk might be slightly shorter than CHUNK_SIZE
  usize chunk_len = min(CHUNK_SIZE, N_IMAGE_BYTES - start);
  cursor = mempcpy(cursor, image_data + start, chunk_len);

  sendPacket(chunk, chunk_len + 2);

  debug(">[DEBUG] done transmiting image chunk with seq #%hu...\n",
        chunk_seq_num);
}

// sends `<CHUNK_SEQ_NUM><CHUNK_BYTES>`
void transmitImageChunks(u16 n_chunks) {
  debug(">[DEBUG] transmiting hu image chunks (%hu chunk/s)...\n", n_chunks);

  for (u16 chunk_i = 0; chunk_i < n_chunks; ++chunk_i)
    transmitSingleImageChunk(chunk_i);

  debug("<[DEBUG] done transmiting image chunks\n");
}

void retransmitMissedChunks() {
  char buffer[256];

  Serial.setTimeout(RETRANSMISSION_TIMEOUT);

  // enable recieve mode to recieve sequence numbers of missed chunks and
  // discard AT command confirmation message
  Serial.write("AT+TEST=RXLRPKT\n", 16);
  Serial.readBytesUntil('\n', buffer, sizeof(buffer));

  // read sequence numbers of missed chunks
  // `MISS<N_MISSED_CHUNKS><SEQ_1><SEQ_2>...<SEQ_N>`
  // where "MISS" is 4 bytes while <N_MISSED_CHUNKS>
  // and <SEQ_i> are 2 bytes each.

  // The AT message recieved has the form `...RX "MISS..."`
  // so we are interested in reading the payload after the `"`
  // and discard everything before that
  Serial.readBytesUntil('"', buffer, sizeof(buffer));

  // discard the first four bytes which contain "MISS"
  Serial.readBytes(buffer, 4);

  // read <N_MISSED_CHUNKS> into buffer
  Serial.readBytes(buffer, 2);
  u16 n_missed_chunks = (buffer[0] << 8) + buffer[1];

  debug("[DEBUG] Number of missed chunks: %hu", n_missed_chunks);

  for (u16 i = 0; i < n_missed_chunks; ++i) {
    // read <SEQ_i> into buffer
    Serial.readBytes(buffer, 2);
    u16 chunk_seq_num = (buffer[0] << 8) + buffer[1];

    debug("[DEBUG] Transmitting missed chunk #%hu/%hu: Seq #%hu\n", i,
          n_missed_chunks, chunk_seq_num);

    // retransmit the image chunk
    delay(RX_SWITCH_DELAY); // TODO: understand why we need this, and maybe it
                            // should be somewhere else (eg outside the loop);
    transmitSingleImageChunk(chunk_seq_num);
  }

  // change timeout back to default value
  Serial.setTimeout(1);
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
  retransmitMissedChunks();
}

void setup() {
  Serial.begin(115200); // TODO: this might need to be 230400
  configLora();
  transmitImage();
}

void loop() {
  // nothing to do here
}
