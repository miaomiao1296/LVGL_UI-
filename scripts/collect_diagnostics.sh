#!/usr/bin/env bash
# Diagnostics script for the 10Khz_Git project.
# Usage: run this from the project root on the remote build host:
#   chmod +x scripts/collect_diagnostics.sh && scripts/collect_diagnostics.sh

set -o pipefail
OUT=diagnostics_output.txt
: > "$OUT"
print() { echo "$@" | tee -a "$OUT"; }
print "DIAGNOSTICS START: $(date)"
print "PWD: $(pwd)"
print "-- git info --"
if command -v git >/dev/null 2>&1; then
  git rev-parse --abbrev-ref HEAD 2>/dev/null || true
  git status --porcelain 2>/dev/null || true
else
  print "git not available"
fi
print "\n-- files (LVGL examples porting) --"
ls -l LVGL/examples/porting 2>/dev/null | tee -a "$OUT" || true
print "\n-- lv_port_disp.c head --"
sed -n '1,80p' LVGL/examples/porting/lv_port_disp.c 2>/dev/null | sed -n '1,80p' | tee -a "$OUT" || true
print "\n-- lv_port_indev.c head --"
sed -n '1,80p' LVGL/examples/porting/lv_port_indev.c 2>/dev/null | sed -n '1,80p' | tee -a "$OUT" || true

print "\n-- CMake configure --"
cmake -S . -B cmake-build-debug- -DCMAKE_BUILD_TYPE=Debug 2>&1 | tee -a "$OUT"
print "\n-- CMake configure done --"

print "\n-- Build (LCD target) --"
cmake --build cmake-build-debug- --target LCD -- -j 4 2>&1 | tee build_output.txt | tee -a "$OUT" || true

print "\n-- Build tail (last 200 lines) --"
tail -n 200 build_output.txt 2>/dev/null | tee -a "$OUT" || true

print "\n-- compile_commands main.c entry --"
if [ -f cmake-build-debug-/compile_commands.json ]; then
  if command -v jq >/dev/null 2>&1; then
    jq -r '.[] | select(.file|endswith("Core/Src/main.c")) | .command' cmake-build-debug-/compile_commands.json 2>/dev/null | tee -a "$OUT" || true
  else
    grep -n '"file": ".*/Core/Src/main.c"' cmake-build-debug-/compile_commands.json -A1 2>/dev/null | tee -a "$OUT" || true
  fi
else
  print "compile_commands.json not found"
fi

print "\n-- LS important dirs timestamps --"
stat -c '%y %n' LVGL/examples/porting/* 2>/dev/null | tee -a "$OUT" || true
stat -c '%y %n' LVGL/src/* 2>/dev/null | tee -a "$OUT" || true

print "\nDIAGNOSTICS END: $(date)"
print "Wrote diagnostics to $OUT"

# Exit with success even if build failed; the log contains the errors for inspection
exit 0

