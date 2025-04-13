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

void configLora() {
#ifdef DEBUG
  Serial.write("AT+LOG=DEBUG\n", 13);
#else
  Serial.write("AT+LOG=QUIET\n", 13);
#endif
  Serial.write("AT+UART=BR, 230400\n", 19);
  Serial.write("AT+MODE=TEST\n", 13);
  Serial.write("AT+TEST=RFCFG,868,SF7,250,12,15,14,ON,OFF,OFF\n", 46);
}

void setup() {
  Serial.begin(115200); // TODO: this might need to be 230400
  configLora();
}

void loop() {
  // nothing to do here
}
