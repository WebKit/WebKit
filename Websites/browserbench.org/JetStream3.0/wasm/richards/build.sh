#!/bin/bash

set -euo pipefail

touch build.log
BUILD_LOG="$(realpath build.log)"
echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee "$BUILD_LOG"

echo "Toolchain versions" | tee -a "$BUILD_LOG"
emcc --version | head -n1 | tee -a "$BUILD_LOG"

echo "Building..." | tee -a "$BUILD_LOG"
mkdir -p build
emcc -o build/richards.js \
    -s WASM=1 -O2 -s TOTAL_MEMORY=83886080 \
    -g1 --emit-symbol-map \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=setupModule -s EXPORTED_FUNCTIONS=_setup,_scheduleIter,_getQpktcount,_getHoldcount \
    richards.c | tee -a "$BUILD_LOG"

echo "Building done" | tee -a "$BUILD_LOG"
