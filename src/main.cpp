#include <Arduino.h>

// Pin map (docs/ASSIGNMENT.md 5.1)
constexpr uint8_t PIN_V1    = 25;
constexpr uint8_t PIN_V2    = 26;
constexpr uint8_t PIN_V3    = 27;
constexpr uint8_t PIN_PUMP  = 33;
constexpr uint8_t PIN_START = 18;
constexpr uint8_t PIN_STOP  = 19;
constexpr uint8_t PIN_RESET = 4;          // not 21/22: keep I2C free for later stages
constexpr uint8_t PIN_DIGITALERROR = 23;
constexpr uint8_t PIN_FLOW  = 13;         // simulated flow sensor: LOW = flow

// Timing in ms (docs/ASSIGNMENT.md 5.2)
constexpr unsigned long T_FLUID_1 = 10000;
constexpr unsigned long T_FLUID_2 = 8000;
constexpr unsigned long T_FLUID_3 = 15000;
constexpr unsigned long SWITCH_DELAY = 3000;  /// valve wait in a changeover

// Flow supervision (docs/ASSIGNMENT.md 6)
constexpr unsigned long FLOW_START_TIMEOUT = 3000;  /// max wait for flow after pump ON
constexpr unsigned long FLOW_STOP_TIMEOUT  = 3000;  /// max wait for flow to stop after pump OFF
constexpr unsigned long FLOW_DEBOUNCE      = 50;    /// FLOW must be stable this long to count

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
  FLUID_3,
  COMPLETE
};

// Global variables
State state = IDLE;
Phase phase = PREPARE_FLUID_1; /// current step of the sequence, only used in RUNNING
int step = 0;                 /// sub-step inside the current phase (used by changeovers)
unsigned long startTime = 0;  /// millis() when the current state, phase or step began
bool errorFlag = false;       /// set when an unsafe condition is detected
bool lastStartLevel = HIGH;   /// previous START level, for edge detection
bool flowStable = false;      /// debounced FLOW state
bool flowRaw = false;         /// last raw FLOW reading
unsigned long flowChangeTime = 0; /// millis() of the last raw FLOW change

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
    case COMPLETE:        return "COMPLETE";
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

  Serial.printf("Pump: %s | V1: %s | V2: %s | V3: %s\r\n",
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
  if (s == RUNNING) return;  // the phases print their own "STATE:" line
  Serial.print("STATE: ");
  Serial.println(stateName(s));
}

void enterPhase(Phase p) {
  phase = p;
  step = 0;
  startTime = millis();
  Serial.print("STATE: ");
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

// Debounce FLOW: accept a new level only after it is stable for FLOW_DEBOUNCE
void updateFlow() {
  bool raw = digitalRead(PIN_FLOW) == LOW;
  if (raw != flowRaw) {
    flowRaw = raw;
    flowChangeTime = millis();
  } else if (raw != flowStable && millis() - flowChangeTime >= FLOW_DEBOUNCE) {
    flowStable = raw;
  }
}

bool flowDetected() {
  return flowStable;
}

// Latches a fault; stateMachine() switches everything OFF and enters ERROR
void raiseError(const char* reason) {
  Serial.print("ERROR: ");
  Serial.println(reason);
  errorFlag = true;
}

// Pump is ON: wait for flow, then run the fluid timer while flow stays present
void fluidPhase(unsigned long duration, Phase next) {
  if (step == 0) {
    if (flowDetected()) {
      Serial.println("Flow detected");
      nextStep();  // fluid timer starts now
    } else if (waitDone(FLOW_START_TIMEOUT)) {
      raiseError("no flow after pump ON");
    }
  } else if (!flowDetected()) {
    raiseError("flow lost during fluid phase");
  } else if (waitDone(duration)) {
    enterPhase(next);
  }
}

// Steps 0-1: pump OFF -> wait until flow stops -> close valve.
// Returns true on the loop the valve is closed.
bool pumpOffThenClose(int valve) {
  if (step == 0) {
    setOutputs(false, valve == 1, valve == 2, valve == 3);  // pump OFF, valve still open
    nextStep();
  } else if (step == 1) {
    if (!flowDetected()) {
      Serial.println("Flow stopped");
      allOutputsOff();                                       // close valve
      return true;
    }
    if (waitDone(FLOW_STOP_TIMEOUT)) raiseError("flow persists after pump OFF");
  }
  return false;
}

// Pump OFF -> wait for no flow -> close valve -> wait -> open next valve -> wait -> pump ON
void changeover(int from, int to, Phase next) {
  if (step <= 1) {
    if (pumpOffThenClose(from)) nextStep();
  } else if (flowDetected()) {
    raiseError("flow while pump OFF");                     // pump is OFF in steps 2-3
  } else if (step == 2 && waitDone(SWITCH_DELAY)) {
    setOutputs(false, to == 1, to == 2, to == 3);         // open next valve
    nextStep();
  } else if (step == 3 && waitDone(SWITCH_DELAY)) {
    setOutputs(true, to == 1, to == 2, to == 3);          // pump ON
    enterPhase(next);
  }
}

void runningState() {
  switch (phase) {
    case PREPARE_FLUID_1:
      // Everything is already OFF: open V1 -> wait -> pump ON
      if (step == 0) {
        setOutputs(false, true, false, false);
        nextStep();
      } else if (flowDetected()) {
        raiseError("flow while pump OFF");
      } else if (waitDone(SWITCH_DELAY)) {
        setOutputs(true, true, false, false);
        enterPhase(FLUID_1);
      }
      break;

    case FLUID_1:
      fluidPhase(T_FLUID_1, CHANGE_1_TO_2);
      break;

    case CHANGE_1_TO_2:
      changeover(1, 2, FLUID_2);
      break;

    case FLUID_2:
      fluidPhase(T_FLUID_2, CHANGE_2_TO_3);
      break;

    case CHANGE_2_TO_3:
      changeover(2, 3, FLUID_3);
      break;

    case FLUID_3:
      fluidPhase(T_FLUID_3, COMPLETE);
      break;

    case COMPLETE:
      if (pumpOffThenClose(3)) {
        Serial.println("All outputs OFF");
        enterState(IDLE);
      }
      break;
  }
}

void stateMachine() {
  bool start = startPressed();  // read every loop to keep edge detection correct
  updateFlow();

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
      // Pump is OFF, so flow means a leak or sensor fault.
      // Grace period after STOP, so the flow can run down first.
      if (flowDetected()) {
        if (waitDone(FLOW_STOP_TIMEOUT)) raiseError("flow detected while idle");
        break;  // no START while there is still flow
      }
      if (start && digitalRead(PIN_STOP) == HIGH) {  // ignore START while STOP is held
        Serial.println("START received");
        enterState(RUNNING);
        enterPhase(PREPARE_FLUID_1);
      }
      break;

    case RUNNING:
      runningState();
      break;

    case ERROR_STATE:
      if (digitalRead(PIN_RESET) == LOW && digitalRead(PIN_DIGITALERROR) == HIGH &&
          !flowDetected()) {
        Serial.println("RESET received");
        errorFlag = false;
        enterState(IDLE);
      }
      break;
  }
}

void setup() {
  for (uint8_t pin : {PIN_PUMP, PIN_V1, PIN_V2, PIN_V3}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_STOP, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_DIGITALERROR, INPUT_PULLUP);
  pinMode(PIN_FLOW, INPUT_PULLUP);

  Serial.begin(115200);
  allOutputsOff();
  Serial.println("SYSTEM READY");
  enterState(IDLE);
}

void loop() {
  stateMachine();
}
