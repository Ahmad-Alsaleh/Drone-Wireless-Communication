#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_endian.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef size_t usize;

usize min(usize a, usize b) { return a < b ? a : b; }
void delay(u32) {}

void *mempcpy(void *dst, const void *src, size_t n) {
  return (char *)memcpy(dst, src, n) + n;
}

class SerialClass {
public:
  void begin(unsigned long baud) {}
  void write(const u8 *bytes, usize length) {
    SerialClass::write((const char *)bytes, length);
  }
  void write(const char *bytes, usize length) {
    for (usize i = 0; i < length; ++i) {
      u8 byte = bytes[i];
      if (byte >= ' ' && byte <= '~')
        printf("%c", byte);
      else if (byte == '\n')
        printf("\\n");
      else if (byte == '\r')
        printf("\\r");
      else if (byte == '\t')
        printf("\\t");
      else
        printf("\\x%02x", byte);
    }
    printf("\n");
  }
  // u8 available() { return 1; }
  u8 readBytesUntil(char terminator, char *buffer, usize length) { return 1; }
  // char read() { return 'A'; }
  void print(const char *msg) { printf("%s", msg); }
  void println(const char *msg) { printf("%s\n", msg); }
} Serial;

void setup();
void loop();

int main() {
  setup();
  // while (true)
  //   loop();
  return 0;
}
