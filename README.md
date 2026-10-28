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

ERROR and RESET use GPIO 23/4 instead of 21/22, so the default I2C pins stay free for displays/sensors in later stages.

## Deliberate deviations from the assignment

- **Changeover has three waits instead of two:** Pump OFF → wait → close valve → wait → open next valve → wait → Pump ON.
  The extra wait lets the pump stop before the valve closes, so it never pumps against a closing valve.
- **`SWITCH_DELAY` = 3000 ms instead of 500 ms**, so every changeover step is clearly visible in the Wokwi simulation.
  Final values follow from the hydraulic prototype (assignment §7).
- **Three top-level states (IDLE, RUNNING, ERROR):** the sequence states from §5.3 (PREPARE_FLUID_1 … FLUID_3, COMPLETE) are phases inside RUNNING.
  The Serial log still prints them as `STATE: FLUID_1` etc., matching §5.6.
