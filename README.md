# FESB – Microcontrollers and Network Embedded Systems

ESP32-based sequential three-fluid spray washing system.

- `docs/ASSIGNMENT.md` – assignment in Markdown
- `docs/ESP32WaterPump.pdf` – original assignment PDF
- `CLAUDE.md` – working rules for Claude (pull → change → push)

Current milestone: **Milestone 2 – Wokwi fault simulation (FLOW input)**.
Milestone 1 (control simulation) is done.

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

## Automated tests (Wokwi CLI)

`test/*.yaml` are Wokwi automation scenarios covering the Milestone 1 and 2 test procedures (full cycle, STOP, reset, flow faults, digital error).
They use `test/diagram.json`: same circuit, but FLOW is a push button, because automation cannot drive a slide switch.

```bash
pio run                                   # build firmware first
WOKWI_CLI_TOKEN=<token> bash test/run_tests.sh
```

Token: https://wokwi.com/dashboard/ci. Never commit it.

## Deliberate deviations from the assignment

- **Flow-supervised changeover** (extends §5.4 with the FLOW input from §6):
  Pump OFF → wait until flow stops (max `FLOW_STOP_TIMEOUT`, else ERROR "flow persists after pump OFF") → close valve → wait → open next valve → wait → Pump ON → wait for flow (max `FLOW_START_TIMEOUT`, else ERROR "no flow after pump ON") → fluid timer starts.
  The valve only closes once the pump has really stopped, and t1/t2/t3 count actual spraying time.
  Losing flow during a fluid phase also gives ERROR (dry running). COMPLETE uses the same pump-OFF → no-flow → close sequence.
  Flow while the pump is OFF (valve waits in a changeover, PREPARE_FLUID_1, or IDLE after a 3 s grace period) also gives ERROR.
- **`SWITCH_DELAY` = 3000 ms instead of 500 ms**, so every changeover step is clearly visible in the Wokwi simulation.
  Final values follow from the hydraulic prototype (assignment §7).
- **Three top-level states (IDLE, RUNNING, ERROR):** the sequence states from §5.3 (PREPARE_FLUID_1 … FLUID_3, COMPLETE) are phases inside RUNNING.
  The Serial log still prints them as `STATE: FLUID_1` etc., matching §5.6.

## Open points for later stages

- **Siphoning (Stage 4, hydraulic prototype):** if a tank sits higher than the nozzle, liquid can keep flowing after Pump OFF by siphon action.
  The flow sensor then reports real flow and the controller raises a false "flow persists after pump OFF" (or "flow while pump OFF") ERROR.
  Check tank vs. nozzle height and the flow run-down time in Stage 4; possible fixes: closing the valve first, an anti-siphon valve, or a longer `FLOW_STOP_TIMEOUT`.
