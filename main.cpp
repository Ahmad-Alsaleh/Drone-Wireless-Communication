#include "fake_image.hpp"

// #define MY_ESP
#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif // DEBUG

#ifdef MY_ESP
#include <Arduino.h> // TODO: try to remove this

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef size_t usize;

#else
#include "utils.h"
#endif // MY_ESP ---

// TODO: replace with real image
#define IMAGE_WIDTH 3
#define IMAGE_HEIGHT 2
#define N_IMAGE_BYTES 764
const u8 image_data[N_IMAGE_BYTES] = {FAKE_IMAGE};

#define CHUNK_SIZE 200
#define RETRANSMISSION_TIMEOUT 5000
#define RX_SWITCH_DELAY 500

// TODO: strip out this loging using macros
// discards bytes in the serial buffer until a terminator.
// Note that this function uses `Serial.read()` internally
// which doesn't respect the Serial timeout value, the function
// will block while the Serial buffer is empty.
void discardSerialBytesUntil(char terminator) {
  char c;
  while (true) {
    if (Serial2.available()) {
      c = Serial2.read();
      Serial.print(c);
      if (c == terminator)
        break;
    }
  }
  Serial.print('\n');
}

// discards a specific number of bytes from the serial buffer.
// Note that this function uses `Serial.read()` internally
// which doesn't respect the Serial timeout value, the function
// will block while the Serial buffer is empty.
void discardNSerialBytes(usize length) {
  while (length--) {
    // block until the buffer has some bytes to read
    while (!Serial2.available())
      ;
    Serial2.read();
  }
}

void configLora() {
  debug(">[DEBUG] sending lora configs...\n");

  Serial2.write("AT+LOG=QUIET\n", 13);
  discardSerialBytesUntil('\n');

  Serial2.write("AT+UART=BR, 230400\n", 19);
  discardSerialBytesUntil('\n');

  Serial2.write("AT+MODE=TEST\n", 13);
  discardSerialBytesUntil('\n');

  Serial2.write("AT+TEST=RFCFG,868,SF7,250,12,15,14,ON,OFF,OFF\n", 46);
  discardSerialBytesUntil('\n');

  debug("<[DEBUG] done sending lora configs\n");
}

void sendAsHex(const u8 *data, usize len) {
  for (usize i = 0; i < len; ++i)
    Serial2.printf("%02X", data[i]);
}

// writes `AT+TEST=TXLRPKT, "<DATA>"\n` to serial as bytes
void sendPacket(const u8 *data, usize len) {
  Serial2.write("AT+TEST=TXLRPKT, \"", 18);
  sendAsHex(data, len);
  Serial2.write("\"\n", 2);

  // discard confirmation message received from AT commands (two lines)
  discardSerialBytesUntil('\n');
  discardSerialBytesUntil('\n');

  // NOTE: Adham transmits the body as hex and UTF-8 encodes the whole buffer
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

  debug(">[DEBUG] transmitting header...\n");

  u8 header[16];
  void *cursor = header;

  // write "LORA" literal to buffer
  cursor = mempcpy(cursor, "LORA", 4);

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

  debug("<[DEBUG] transmitting header\n");
}

// transmits a single chunk in the following format:
// `<CHUNK_SEQ_NUM><CHUNK_BYTES>` where <CHUNK_SEQ_NUM> is 2 bytes
// and <CHUNK_BYTES> is `CHUNK_SIZE` bytes (or maybe less for the last chunk)
void transmitSingleImageChunk(u16 chunk_seq_num) {
  debug(">[DEBUG] transmitting image chunk with seq #%hu...\n", chunk_seq_num);

  u8 chunk[CHUNK_SIZE + 2];
  void *cursor = chunk;

  // the chunk start index in the image's array of bytes
  usize chunk_start_i = chunk_seq_num * CHUNK_SIZE;

  // write <CHUNK_SEQ_NUM> to buffer
  chunk_seq_num = htons(chunk_seq_num);
  cursor = mempcpy(cursor, &chunk_seq_num, sizeof(chunk_seq_num));

  // write <CHUNK_BYTES> to the buffer.
  // the last chunk might be slightly shorter than `CHUNK_SIZE`,
  // so `min` is used.

  usize chunk_len = min((usize)CHUNK_SIZE, N_IMAGE_BYTES - chunk_start_i);
  cursor = mempcpy(cursor, image_data + chunk_start_i, chunk_len);

  sendPacket(chunk, chunk_len + 2);

  debug("<[DEBUG] done transmitting image chunk\n");
}

// transmits the image as chunks
void transmitImageChunks(u16 n_chunks) {
  debug(">[DEBUG] transmitting image chunks (%hu chunk/s)...\n", n_chunks);

  for (u16 chunk_i = 0; chunk_i < n_chunks; ++chunk_i)
    transmitSingleImageChunk(chunk_i);

  debug("<[DEBUG] done transmitting image chunks\n");
}

// reads from Serial the sequence numbers of missed chunks
// and retransmits the bytes of the missed chunks
void retransmitMissedChunks() {
  Serial2.setTimeout(RETRANSMISSION_TIMEOUT);

  // enable receive mode to receive sequence numbers of missed chunks
  Serial2.write("AT+TEST=RXLRPKT\n", 16);
  // discard confirmation message received from AT commands
  discardSerialBytesUntil('\n');

  // the AT message received has the form:
  // +TEST: LEN:<LEN>, RSSI:<RSSI>, SNR:<SNR>\r\n
  // +TEST: RX "<PAYLOAD>"\r\n

  // we are interested in the payload after the first `"` in the second line.
  // discard the first line and the bytes before the <PAYLOAD>.
  discardSerialBytesUntil('\n');
  discardSerialBytesUntil('"');

  // the payload received has the form:
  // `MISS<N_MISSED_CHUNKS><SEQ_1><SEQ_2>...<SEQ_N>`
  // where <N_MISSED_CHUNKS> and <SEQ_i> are 2 bytes each.

  // discard the four bytes of the "MISS" literal
  discardNSerialBytes(8);

  // read and parse the two bytes of <N_MISSED_CHUNKS>.
  // note that ther protocol uses big endian.
  u16 n_missed_chunks = (Serial2.read() << 8) | Serial2.read();

  debug("[DEBUG] Number of missed chunks: %hu", n_missed_chunks);

  // TODO: instead of malloc, we can use a static array of
  // size, say, 256 bytes here since according to @Yousef an
  // image is at most 15KB which is 15000 / 200 = 75 chunks.
  // Thus 256 bytes should more than enough

  // the received payload has `n_missed_chunks` sequence numbers
  // and each sequence number is 2 bytes.
  u8 *buffer = (u8 *)malloc(2 * n_missed_chunks);

  // read the sequence numbers of the missed chunks
  Serial2.readBytes(buffer, 2 * n_missed_chunks);

  // TODO: understand why we need this delay, and whether it should be inside
  // the loop

  // waiting before sending missed chunks
  delay(RX_SWITCH_DELAY);

  // retransmit the missed chunks
  for (u16 missed_chunk_i = 0; missed_chunk_i < n_missed_chunks;
       ++missed_chunk_i) {

    u16 chunk_seq_num =
        (buffer[2 * missed_chunk_i] << 8) | buffer[2 * missed_chunk_i + 1];

    debug("[DEBUG] Transmitting missed chunk #%hu/%hu: Seq #%hu\n",
          missed_chunk_i, n_missed_chunks, chunk_seq_num);

    transmitSingleImageChunk(chunk_seq_num);
  }

  // switch Serial timeout back to default value
  Serial2.setTimeout(1000);
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

  Serial.begin(115200);
  Serial2.begin(230400, SERIAL_8N1, 16, 17);

  configLora();
  transmitImage();
}

void loop() {
  // nothing to do here
}
