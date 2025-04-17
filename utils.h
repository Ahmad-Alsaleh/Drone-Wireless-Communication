#ifndef UTILS
#define UTILS

#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_endian.h>
#include <thread>
#include <unistd.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef size_t usize;

usize min(usize a, usize b) { return a < b ? a : b; }
void delay(u32 milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void *mempcpy(void *dst, const void *src, size_t n) {
  return (char *)memcpy(dst, src, n) + n;
}

class SerialClass {
public:
  void begin(long baud) { printf("[DEBUG] Serial.begin(%lu)", baud); }
  usize write(const u8 *bytes, usize length) {
    return SerialClass::write((const char *)bytes, length);
  }
  usize write(const char *bytes, usize length) {
    for (usize i = 0; i < length; ++i) {
      u8 byte = bytes[i];
      if (byte == '\n')
        printf("\\n");
      else if (byte == '\r')
        printf("\\r");
      else if (byte == '\t')
        printf("\\t");
      if (byte == '\\')
        printf("\\\\");
      else if (byte >= ' ' && byte <= '~')
        printf("%c", byte);
      else
        printf("\\x%02x", byte);
    }
    printf("\n");

    return length;
  }
  // TODO: have these read methods read from a file
  u8 readBytes(u8 *buffer, usize length) { return 1; }
  u8 readBytes(char *buffer, usize length) {
    return SerialClass::readBytes((u8 *)buffer, length);
  }
  u8 readBytesUntil(char terminator, u8 *buffer, usize length) { return 1; }
  u8 readBytesUntil(char terminator, char *buffer, usize length) {
    return SerialClass::readBytesUntil(terminator, (u8 *)buffer, length);
  }
  int read() { return 1; }
  int available() { return 1; }
  void setTimeout(long milliseconds) {
    printf("[DEBUG] Serial.setTimeout(%lu)", milliseconds);
  }
} Serial;

void setup();
void loop();

int main() {
  setup();
  // while (true)
  //   loop();
  return 0;
}

#endif
