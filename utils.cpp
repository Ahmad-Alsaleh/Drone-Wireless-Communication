#include "utils.h"
#include <stdint.h>
#include <stdio.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

void SerialClass::begin(unsigned long baud) {
  printf("Starting Serial (%ld)\n", baud);
}

void SerialClass::write(const u8 *bytes, size_t length) {
  for (size_t i = 0; i < length; ++i)
    printf("%02X ", bytes[i]);
  printf("\n");
}

void SerialClass::print(const char *msg) { printf("%s", msg); }

void SerialClass::println(const char *msg) { printf("%s\n", msg); }

void write_u32(u8 *buffer, u32 value) {
  for (u8 i = 0; i < 4; ++i)
    buffer[i] = (value >> (3 - i) * 8) & 0xFF;
}

void write_u16(u8 *buffer, u16 value) {
  buffer[0] = (value >> 8) & 0xFF;
  buffer[1] = value & 0xFF;
}
