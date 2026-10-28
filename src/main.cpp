#include <Arduino.h>

// Pin map (docs/ASSIGNMENT.md 5.1)
constexpr uint8_t PIN_V1    = 25;
constexpr uint8_t PIN_V2    = 26;
constexpr uint8_t PIN_V3    = 27;
constexpr uint8_t PIN_PUMP  = 33;
constexpr uint8_t PIN_START = 18;
constexpr uint8_t PIN_STOP  = 19;
constexpr uint8_t PIN_RESET = 21;
constexpr uint8_t PIN_DIGITALERROR = 22;

// Timing in ms (docs/ASSIGNMENT.md 5.2)
constexpr unsigned long T_FLUID_1 = 10000;
constexpr unsigned long T_FLUID_2 = 8000;
constexpr unsigned long T_FLUID_3 = 15000;
constexpr unsigned long VALVE_SWITCH_DELAY = 500;

enum State {
  IDLE,
  RUNNING,
  ERROR_STATE
};

// Steps of the wash sequence inside RUNNING
enum Phase {
  PREPARE_FLUID_1,
  FLUID_1,
  CHANGE_1_TO_2,
  FLUID_2,
  CHANGE_2_TO_3,
  FLUID_3
};

// Global variables
State state = IDLE;
Phase phase = PREPARE_FLUID_1; /// current step of the sequence, only used in RUNNING
int step = 0;                 /// sub-step inside the current phase (used by changeovers)
unsigned long startTime = 0;  /// millis() when the current state, phase or step began
bool errorFlag = false;       /// set when an unsafe condition is detected
bool lastStartLevel = HIGH;   /// previous START level, for edge detection

const char* stateName(State s) {
  switch (s) {
    case IDLE:        return "IDLE";
    case RUNNING:     return "RUNNING";
    case ERROR_STATE: return "ERROR";
  }
  return "?";
}

const char* phaseName(Phase p) {
  switch (p) {
    case PREPARE_FLUID_1: return "PREPARE_FLUID_1";
    case FLUID_1:         return "FLUID_1";
    case CHANGE_1_TO_2:   return "CHANGE_1_TO_2";
    case FLUID_2:         return "FLUID_2";
    case CHANGE_2_TO_3:   return "CHANGE_2_TO_3";
    case FLUID_3:         return "FLUID_3";
  }
  return "?";
}

// Only place that writes the outputs, so the safety rules are enforced here
void setOutputs(bool pump, bool v1, bool v2, bool v3) {
  bool unsafe = (v1 + v2 + v3 > 1) || (pump && !(v1 || v2 || v3));
  if (unsafe) {
    pump = v1 = v2 = v3 = false;
    errorFlag = true;
    Serial.println("ERROR: unsafe output combination blocked");
  }

  // Pump OFF before valves change, pump ON only after valves are set
  if (!pump) digitalWrite(PIN_PUMP, LOW);
  digitalWrite(PIN_V1, v1);
  digitalWrite(PIN_V2, v2);
  digitalWrite(PIN_V3, v3);
  if (pump) digitalWrite(PIN_PUMP, HIGH);

  Serial.printf("Pump: %s | V1: %s | V2: %s | V3: %s\n",
                pump ? "ON" : "OFF", v1 ? "ON" : "OFF",
                v2 ? "ON" : "OFF", v3 ? "ON" : "OFF");
}

void allOutputsOff() {
  setOutputs(false, false, false, false);
}

void enterState(State s) {
  state = s;
  step = 0;
  startTime = millis();
  Serial.print("STATE: ");
  Serial.println(stateName(s));
}

void enterPhase(Phase p) {
  phase = p;
  step = 0;
  startTime = millis();
  Serial.print("PHASE: ");
  Serial.println(phaseName(p));
}

void nextStep() {
  step++;
  startTime = millis();  // restart the timer once, on step change
}

bool waitDone(unsigned long ms) {
  return millis() - startTime >= ms;
}

// True only on the press (HIGH -> LOW), not while the button is held
bool startPressed() {
  bool level = digitalRead(PIN_START);
  bool pressed = (lastStartLevel == HIGH && level == LOW);
  lastStartLevel = level;
  return pressed;
}

// Pump OFF + close valve -> wait -> open next valve -> wait -> pump ON
void changeover(int fluid, Phase next) {
  bool v1 = (fluid == 1), v2 = (fluid == 2), v3 = (fluid == 3);

  if (step == 0) {
    allOutputsOff();
    nextStep();
  } else if (step == 1 && waitDone(VALVE_SWITCH_DELAY)) {
    setOutputs(false, v1, v2, v3);
    nextStep();
  } else if (step == 2 && waitDone(VALVE_SWITCH_DELAY)) {
    setOutputs(true, v1, v2, v3);
    enterPhase(next);
  }
}

void runningState() {
  switch (phase) {
    case PREPARE_FLUID_1:
      changeover(1, FLUID_1);
      break;

    case FLUID_1:
      if (waitDone(T_FLUID_1)) enterPhase(CHANGE_1_TO_2);
      break;

    case CHANGE_1_TO_2:
      changeover(2, FLUID_2);
      break;

    case FLUID_2:
      if (waitDone(T_FLUID_2)) enterPhase(CHANGE_2_TO_3);
      break;

    case CHANGE_2_TO_3:
      changeover(3, FLUID_3);
      break;

    case FLUID_3:
      if (waitDone(T_FLUID_3)) {  // run complete
        allOutputsOff();
        Serial.println("All outputs OFF");
        enterState(IDLE);
      }
      break;
  }
}

void stateMachine() {
  bool start = startPressed();  // read every loop to keep edge detection correct

  // Faults latch into ERROR until RESET is pressed
  if (digitalRead(PIN_DIGITALERROR) == LOW && state != ERROR_STATE) {
    Serial.println("ERROR: digital error input active");
    errorFlag = true;
  }
  if (errorFlag && state != ERROR_STATE) {
    allOutputsOff();
    enterState(ERROR_STATE);
    return;
  }

  // STOP has priority while running
  if (digitalRead(PIN_STOP) == LOW && state == RUNNING) {
    Serial.println("STOP received");
    allOutputsOff();
    enterState(IDLE);
    return;
  }

  switch (state) {
    case IDLE:
      if (start) {
        Serial.println("START received");
        enterState(RUNNING);
        enterPhase(PREPARE_FLUID_1);
      }
      break;

    case RUNNING:
      runningState();
      break;

    case ERROR_STATE:
      if (digitalRead(PIN_RESET) == LOW && digitalRead(PIN_DIGITALERROR) == HIGH) {
        Serial.println("RESET received");
        errorFlag = false;
        enterState(IDLE);
      }
      break;
  }
}

void setup() {
  // Drive outputs LOW before enabling them, so nothing glitches ON at reset
  for (uint8_t pin : {PIN_PUMP, PIN_V1, PIN_V2, PIN_V3}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_STOP, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_DIGITALERROR, INPUT_PULLUP);

  Serial.begin(115200);
  allOutputsOff();
  Serial.println("SYSTEM READY");
  enterState(IDLE);
}

void loop() {
  stateMachine();
}
