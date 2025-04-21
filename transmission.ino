
#include <Arduino.h> // TODO: try to remove this
#include <string.h>

#define USE_FAKE

#ifndef USE_FAKE
#include <Seeed_Arduino_SSCMA.h>
SSCMA AI;
#else
#include <fake_image.hpp>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef size_t usize;

#define DEBUG
#ifdef DEBUG
#define debug(...) // Serial.printf(__VA_ARGS__)
#else
#define debug(...)
#endif // DEBUG

#ifndef USE_FAKE
#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480
usize n_image_bytes = 0;
char *image_data;
#else
#define IMAGE_WIDTH 3
#define IMAGE_HEIGHT 2
usize n_image_bytes = 764;
#define FAKE_IMAGE
const char image_data[N_IMAGE_BYTES] = {FAKE_IMAGE};
#endif

#define CHUNK_SIZE 200
#define RETRANSMISSION_TIMEOUT 5000
#define RX_SWITCH_DELAY 500

usize min(usize a, usize b)
{
  return a < b ? a : b;
}

void configLora()
{
  debug(">[DEBUG] sending lora configs...\n");

  // TODO: read after every write to discard confirmation msg
  Serial.write("AT+LOG=QUIET\n", 13);
  discardSerialBytesUntil('\n');

  Serial.write("AT+UART=BR, 230400\n", 19);
  discardSerialBytesUntil('\n');
  Serial.write("AT+MODE=TEST\n", 13);
  discardSerialBytesUntil('\n');
  Serial.write("AT+TEST=RFCFG,868,SF7,250,12,15,14,ON,OFF,OFF\n", 46);
  discardSerialBytesUntil('\n');

  debug("<[DEBUG] done sending lora configs\n");
}

// discards bytes in the serial buffer until a terminator.
// Note that this function uses `Serial.read()` internally
// which doesn't respect the Serial timeout value, the function
// will block while the Serial buffer is empty.
void discardSerialBytesUntil(char terminator)
{
  char c;
  while (true)
  {
    if (Serial.available())
    {
      c = Serial.read();
      // Serial.print(c);  // Log the byte

      if (c == terminator)
      {
        break;
      }
    }
  }
  // Serial.print("\n");
}

// discards a specific number of bytes from the serial buffer.
// Note that this function uses `Serial.read()` internally
// which doesn't respect the Serial timeout value, the function
// will block while the Serial buffer is empty.
void discardNSerialBytes(usize length)
{
  while (length--)
  {
    // block until the buffer has some bytes to read
    while (!Serial.available())
      ;
    Serial.read();
  }
}

void sendAsHex(const u8 *data, usize len)
{
  for (usize i = 0; i < len; ++i)
    Serial2.printf("%02X", data[i]);
}

// writes `AT+TEST=TXLRPKT, "<DATA>"\n` to serial as bytes
void sendPacket(const u8 *data, usize len)
{
  Serial.write("AT+TEST=TXLRPKT, \"", 18);
  sendAsHex(data, len);
  Serial.write("\"\n", 2);

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
void transmitHeader(u32 n_bytes_to_send)
{
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
void transmitSingleImageChunk(u16 chunk_seq_num)
{
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
  usize chunk_len = min(CHUNK_SIZE, n_image_bytes - chunk_start_i);
  cursor = mempcpy(cursor, image_data + chunk_start_i, chunk_len);

  sendPacket(chunk, chunk_len + 2);

  debug(">[DEBUG] done transmitting image chunk\n");
}

// transmits the image as chunks
void transmitImageChunks(u16 n_chunks)
{
  debug(">[DEBUG] transmitting image chunks (%hu chunk/s)...\n", n_chunks);

  for (u16 chunk_i = 0; chunk_i < n_chunks; ++chunk_i)
  {
    if (chunk_i == 2)
      continue;
    transmitSingleImageChunk(chunk_i);
  }

  debug("[>DEBUG] done transmitting image chunks\n");
}

char unhex(char *hexbyte)
{
  char c;
  sscanf(hexbyte, "%2x", &c);
  return c;
}

int hexStringToByteArray(const char *hexString, char *byteArray, size_t hexLength)
{
  int bytesConverted = 0;

  unsigned int value;
  // Process two hex characters at a time
  for (size_t i = 0; i < hexLength; i += 2)
  {
    sscanf(&hexString[i], "%2x", &value);
    byteArray[bytesConverted++] = (u8)value;
  }
  return bytesConverted;
}

// reads from Serial the sequence numbers of missed chunks
// and retransmits the bytes of the missed chunks
void retransmitMissedChunks()
{
  Serial.setTimeout(RETRANSMISSION_TIMEOUT);

  // enable receive mode to receive sequence numbers of missed chunks
  Serial.write("AT+TEST=RXLRPKT\n", 16);
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
  // note that every byte is sent as two ASCII chars
  discardNSerialBytes(8);

  // read and parse the two bytes of <N_MISSED_CHUNKS>.
  // note that every byte is represented as two bytes
  // eg: the byte 0xA4 is sent as two ASCII bytes ['A', '4']
  char higher_bytes[2];
  Serial.readBytes(higher_bytes, 2);

  char lower_bytes[2];
  Serial.readBytes(lower_bytes, 2);

  u16 n_missed_chunks = (unhex(higher_bytes) << 8) | unhex(lower_bytes);

  debug("[DEBUG] Number of missed chunks: %hu\n", n_missed_chunks);

  if (n_missed_chunks == 0)
    return;

  // TODO: instead of malloc, we can use a static array of
  // size, say, 256 bytes here since according to @Yousef an
  // image is at most 15KB which is 15000 / 200 = 75 chunks.
  // Thus 256 bytes should more than enough

  // the received payload has `n_missed_chunks` sequence numbers
  // and each sequence number is 2 bytes and every byte is represented
  // as two ASCII bytes
  u8 *buffer = (u8 *)malloc(2 * 2 * n_missed_chunks);

  // read the sequence numbers of the missed chunks
  // note that every byte is two ASCII charachters
  Serial.readBytes(buffer, 2 * 2 * n_missed_chunks);

  // TODO: understand why we need this delay, and whether it should be inside
  // the loop

  // waiting before sending missed chunks
  delay(RX_SWITCH_DELAY);

  // retransmit the missed chunks
  for (u16 missed_chunk_i = 0; missed_chunk_i < n_missed_chunks; ++missed_chunk_i)
  {
    u8 *first_byte = buffer + 4 * missed_chunk_i;

    u16 chunk_seq_num =
        (unhex((char *)first_byte) << 8) | unhex((char *)first_byte + 2);

    debug("[DEBUG] Transmitting missed chunk #%hu/%hu: Seq #%hu\n",
          missed_chunk_i, n_missed_chunks, chunk_seq_num);

    transmitSingleImageChunk(chunk_seq_num);
  }

  // switch Serial timeout back to default value
  Serial.setTimeout(1000);

  free(buffer);
}

void transmitImage()
{
  // manually implement the ceil function
  u16 n_chunks = n_image_bytes / CHUNK_SIZE;
  if (n_image_bytes != n_chunks * CHUNK_SIZE)
  {
    ++n_chunks;
  }

  u32 n_bytes_to_send = n_image_bytes + 2 * n_chunks;

  transmitHeader(n_bytes_to_send);
  transmitImageChunks(n_chunks);
  retransmitMissedChunks();
}

void setup()
{
  AI.begin();
  pinMode(1, OUTPUT);

  // Serial.begin(115200);  // TODO: this might need to be 230400
  Serial.begin(230400); //, SERIAL_8N1, 16, 17);
  configLora();

#ifdef USE_FAKE
  transmitImage();
#else
  delay(500);
  if (!AI.invoke(1, false, true))
  {
    if (AI.last_image().length() > 0)
    {
      String img = AI.last_image();
      image_data = (char *)img.c_str();
      n_image_bytes = img.length();
      delay(500);
      transmitImage();
    }
  }
#endif
}

void loop()
{
  // nothing to do here
}
