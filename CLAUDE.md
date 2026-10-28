# CLAUDE.md

Instructions for every Claude session (Claude Code and Claude in claude.ai) working in this repo.
This repo is the shared channel between Claude Code and Claude chat, so both work on the same files.

## Git workflow (mandatory)

1. **Pull first.** At the start of every session, and again right before you change anything:
   `git pull --rebase origin main`
2. **Then make your change.** Keep changes small and focused.
3. **Push immediately.** Commit and push directly after each change — do not leave local, unpushed work:
   `git add -A && git commit -m "<what changed>" && git push origin main`
4. If the push is rejected (someone else pushed in between): `git pull --rebase origin main`, resolve conflicts, push again. Never force-push.
5. Work on `main` unless the user asks for a branch.

## Project

ESP32-based sequential three-fluid spray washing system, course Microcontrollers and Network Embedded Systems (FESB, Split).

- Assignment: `docs/ASSIGNMENT.md` (Markdown) and `docs/ESP32WaterPump.pdf` (original, source of truth).
- Current focus: **Milestone 1 – Wokwi control simulation** (state machine, START/STOP, safety rules, Serial diagnostics).

Key constraints from the assignment:
- Pins: V1 = GPIO25, V2 = GPIO26, V3 = GPIO27, Pump = GPIO33, START = GPIO18, STOP = GPIO19.
- `millis()`-based non-blocking timing, no long `delay()`.
- Only one valve ON at a time; pump never ON with all valves closed.
- STOP overrides every state and switches everything OFF immediately.
- All outputs OFF on power-up/reset and after COMPLETE.
- Changeover: Pump OFF → close valve → wait → open next valve → wait → Pump ON.

## User

Finn — experienced with ESP32, FreeRTOS, Arduino, Modbus. Communicate in Dutch, keep technical terms and code comments in English. Be concise and direct. If a request is unclear, ask before acting.
