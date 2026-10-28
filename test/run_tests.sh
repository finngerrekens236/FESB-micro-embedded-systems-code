#!/usr/bin/env bash
# Runs every Wokwi scenario in test/ against the current firmware build.
# Needs WOKWI_CLI_TOKEN and a build in .pio/build/esp32dev (pio run).
cd "$(dirname "$0")/.."
CLI="${WOKWI_CLI:-$HOME/.wokwi/bin/wokwi-cli.exe}"
pass=0; fail=0
for s in test/*.yaml; do
  name=$(basename "$s" .yaml)
  if "$CLI" --timeout 120000 --diagram-file test/diagram.json --scenario "$s" . > "test/$name.log" 2>&1; then
    echo "PASS  $name"; pass=$((pass+1))
  else
    echo "FAIL  $name  (see test/$name.log)"; fail=$((fail+1))
  fi
done
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
