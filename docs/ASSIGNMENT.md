# ESP32-Based Sequential Three-Fluid Spray Washing System

Markdown version of `docs/ESP32WaterPump.pdf` (source of truth). If the two ever disagree, the PDF wins.

## 1. Project idea and objective

Automated washing system that applies **three different liquids sequentially** through **one common pump** and one or more spray nozzles. Each liquid has its own reservoir. Electrically controlled inlet valves select which reservoir is connected to the pump. An **ESP32** controls valves, pump, timing sequence, safety interlocks and (later) sensors and a user interface.

Process: Fluid 1 for t1 → controlled changeover → Fluid 2 for t2 → controlled changeover → Fluid 3 for t3 → cycle complete.
**Only one process-fluid valve may be open at a time.**

## 2. Target system architecture

Hydraulic:

```
Tank 1 ─ Filter ─ Valve 1 ─┐
Tank 2 ─ Filter ─ Valve 2 ─┼─ Manifold ─ Pump ─ Flow/Pressure sensing ─ Nozzle(s)
Tank 3 ─ Filter ─ Valve 3 ─┘
```

Electrical:

```
12 V DC ─┬─ Pump
         ├─ Valve 1
         ├─ Valve 2
         ├─ Valve 3
         └─ DC/DC converter → 5 V → ESP32
ESP32 GPIO → output drivers → pump / valves
```

First prototype may use a relay for the pump; final system should use logic-level MOSFET drivers. Every inductive load needs flyback suppression.

## 3. Suggested core components

| Subsystem | Component | Notes |
|---|---|---|
| Controller | ESP32 DevKit | Main state-machine controller; Wi-Fi for later stages |
| Pump | Existing CrocSee 12 V diaphragm pump | One common pump for all three liquids |
| Inlet selection | 3 × 12 V DC normally-closed direct-acting solenoid valves | Prefer zero differential-pressure type; verify chemical compatibility |
| Output drivers | Relay for early test; logic-level N-MOSFETs for final design | MOSFET is silent and suited to frequent switching |
| Protection | Flyback diode per inductive load | Across pump and each solenoid coil |
| Logic power | 12 V → 5 V DC/DC converter | Set and verify 5.0 V before connecting ESP32 |
| Local filtering | 470 µF electrolytic + 100 nF ceramic | Near ESP32 power input |
| Controls | START and STOP pushbuttons | STOP overrides all process states |

## 4. Development strategy (stages)

1. **Wokwi control simulation** — ESP32, 3 valve outputs, pump output, START/STOP. State machine + safety interlocks, no hardware.
2. **Wokwi fault simulation** — simulated flow input + ERROR state. Loss of flow, STOP and reset must always force a safe state.
3. **Bench electrical prototype** — firmware on a real ESP32; test DC/DC, relay/MOSFET outputs, one real load at a time, no liquids.
4. **Hydraulic prototype with water** — 3 reservoirs, 3 valves, manifold, pump, nozzle. Water (+ food colouring) to visualise transitions and measure dead volume.
5. **Process characterisation** — priming time, flow, valve restriction, transition time between fluids; decide if purge intervals are needed.
6. **Final system functions** — flow/pressure/level sensing, alarms, MOSFET drivers, Wi-Fi interface, recipe parameters, battery operation, enclosure.

## 5. Milestone 1 – Wokwi control simulation

Done when the complete automatic sequence runs in Wokwi with **LEDs instead of valves and pump**, correct timing, and safe response to START, STOP and ESP32 reset.

### 5.1 Simulation I/O

| Function | Wokwi component | ESP32 pin | Meaning |
|---|---|---|---|
| Valve 1 | LED + resistor | GPIO 25 | ON = Fluid 1 selected |
| Valve 2 | LED + resistor | GPIO 26 | ON = Fluid 2 selected |
| Valve 3 | LED + resistor | GPIO 27 | ON = Fluid 3 selected |
| Pump | LED + resistor | GPIO 33 | ON = pump commanded ON |
| START | Pushbutton | GPIO 18 | Starts an automatic cycle |
| STOP | Pushbutton | GPIO 19 | Immediately stops the cycle |

### 5.2 Initial test parameters (simulation only)

`t1 = 10 s`, `t2 = 8 s`, `t3 = 15 s`, `valveSwitchDelay = 500 ms`. Final values are determined experimentally later.

### 5.3 Required state machine

```
IDLE
 ↓ START
PREPARE_FLUID_1
 ↓
FLUID_1
 ↓ t1 elapsed
CHANGE_1_TO_2
 ↓
FLUID_2
 ↓ t2 elapsed
CHANGE_2_TO_3
 ↓
FLUID_3
 ↓ t3 elapsed
COMPLETE
 ↓
IDLE
```

Use **`millis()`-based non-blocking timing**, no long `delay()` calls, so STOP, sensors and future faults are evaluated continuously.

### 5.4 Changeover logic

```
Pump OFF
→ close current valve
→ wait valveSwitchDelay
→ open next valve
→ wait valveSwitchDelay
→ Pump ON
```

### 5.5 Safety rules

- On power-up or reset: Pump OFF, V1 OFF, V2 OFF, V3 OFF.
- Never two process-fluid valves ON simultaneously.
- In normal operation, the pump may not run when all inlet valves are closed.
- STOP has priority over every state and immediately switches pump and all valves OFF.
- After t3, all outputs OFF before returning to IDLE.
- START is accepted only in IDLE (unless a restart policy is defined later).

### 5.6 Serial Monitor diagnostics

Print current state and relevant events, e.g.:

```
SYSTEM READY
START received
STATE: FLUID_1
Pump: ON | V1: ON | V2: OFF | V3: OFF
STATE: CHANGE_1_TO_2
STATE: FLUID_2
...
STATE: COMPLETE
All outputs OFF
STATE: IDLE
```

### 5.7 Test procedure

Pass condition for every test: behaviour matches the defined safety and sequence rules.

| Test | Procedure |
|---|---|
| Power-up | Start simulation; all four output LEDs OFF |
| Normal cycle | Press START; V1+Pump, then V2+Pump, then V3+Pump in correct order |
| Mutual exclusion | Observe full cycle; never two valve LEDs ON at once |
| STOP during Fluid 1 | Press STOP during Fluid 1; all LEDs OFF immediately |
| STOP during Fluid 2/3 | Repeat in later stages; identical response |
| Reset | Reset ESP32 during an active stage; after restart all outputs OFF |
| Repeated cycle | Several full cycles; deterministic, correct return to IDLE |

### 5.8 Definition of done

Reliable full t1 → t2 → t3 sequence, valve mutual exclusion enforced, immediate reaction to STOP, safe start after reset, understandable state diagnostics on Serial Monitor.

## 6. Milestone 2 preview – simulated flow fault

Add one simulated FLOW input. When the pump is commanded ON, a flow signal is expected within a configurable timeout (e.g. 3 s). If no flow: stop pump, close all valves, enter **ERROR** state.

## 7. Not to be finalised yet

Exact process times, transition/purge times, valve model, flow sensor, pressure sensor, battery system. These depend on physical measurements. The common line between manifold and nozzle has dead volume, so Fluid 2 does not reach the nozzle immediately after Valve 2 opens; measure this in the hydraulic prototype stage.

## 8. Immediate next action

Build the Wokwi circuit: one ESP32, four LEDs, LED resistors, two pushbuttons. Implement the state machine and complete the Milestone 1 test procedure before connecting real valves or pump.

> Note: the PDF says "six LED resistors" for four LEDs; four series resistors are sufficient for four LEDs (buttons use internal pull-ups).
