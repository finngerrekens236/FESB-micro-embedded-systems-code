# FESB – Microcontrollers and Network Embedded Systems

ESP32-based sequential three-fluid spray washing system.

- `docs/ASSIGNMENT.md` – assignment in Markdown
- `docs/ESP32WaterPump.pdf` – original assignment PDF
- `CLAUDE.md` – working rules for Claude (pull → change → push)

Current milestone: **Milestone 1 – Wokwi control simulation**.

## Wokwi pin map

| Function | GPIO |
|---|---|
| Valve 1 / 2 / 3 (LED) | 25 / 26 / 27 |
| Pump (LED) | 33 |
| START / STOP | 18 / 19 |
| DIGITAL ERROR (test input) | 23 |
| RESET (leaves ERROR) | 4 |
| FLOW (slide switch, LOW = flow) | 13 |

ERROR and RESET use GPIO 23/4 instead of 21/22, so the default I2C pins stay free for displays/sensors in later stages.

## Deliberate deviations from the assignment

- **Flow-supervised changeover** (extends §5.4 with the FLOW input from §6):
  Pump OFF → wait until flow stops (max `FLOW_STOP_TIMEOUT`, else ERROR "flow persists after pump OFF") → close valve → wait → open next valve → wait → Pump ON → wait for flow (max `FLOW_START_TIMEOUT`, else ERROR "no flow after pump ON") → fluid timer starts.
  The valve only closes once the pump has really stopped, and t1/t2/t3 count actual spraying time.
  Losing flow during a fluid phase also gives ERROR (dry running). COMPLETE uses the same pump-OFF → no-flow → close sequence.
- **`SWITCH_DELAY` = 3000 ms instead of 500 ms**, so every changeover step is clearly visible in the Wokwi simulation.
  Final values follow from the hydraulic prototype (assignment §7).
- **Three top-level states (IDLE, RUNNING, ERROR):** the sequence states from §5.3 (PREPARE_FLUID_1 … FLUID_3, COMPLETE) are phases inside RUNNING.
  The Serial log still prints them as `STATE: FLUID_1` etc., matching §5.6.
