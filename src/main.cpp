#include <Arduino.h>

// Pin map (docs/ASSIGNMENT.md 5.1)
constexpr uint8_t PIN_V1    = 25;
constexpr uint8_t PIN_V2    = 26;
constexpr uint8_t PIN_V3    = 27;
constexpr uint8_t PIN_PUMP  = 33;
constexpr uint8_t PIN_START = 18;
constexpr uint8_t PIN_STOP  = 19;

void allOutputsOff() {
  digitalWrite(PIN_PUMP, LOW);
  digitalWrite(PIN_V1, LOW);
  digitalWrite(PIN_V2, LOW);
  digitalWrite(PIN_V3, LOW);
}

void setup() {
  // Drive outputs LOW before enabling them, so nothing glitches ON at reset
  for (uint8_t pin : {PIN_PUMP, PIN_V1, PIN_V2, PIN_V3}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_STOP, INPUT_PULLUP);
  allOutputsOff();

  Serial.begin(115200);
  Serial.println("SYSTEM READY");
}

void loop() {
  // TODO: state machine (Milestone 1)
}
