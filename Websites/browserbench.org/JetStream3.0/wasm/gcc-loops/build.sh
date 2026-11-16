#!/bin/bash

set -euo pipefail

touch build.log
BUILD_LOG="$(realpath build.log)"
echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee "$BUILD_LOG"

echo "Toolchain versions" | tee -a "$BUILD_LOG"
emcc --version | head -n1 | tee -a "$BUILD_LOG"

echo "Building..." | tee -a "$BUILD_LOG"
mkdir -p build
emcc -o build/gcc-loops.js \
    -s WASM=1 -O2 \
    -g1 --emit-symbol-map \
    -s MODULARIZE=1 -s EXPORT_NAME=setupModule \
    -DSMALL_PROBLEM_SIZE=1 \
    gcc-loops.cpp | tee -a "$BUILD_LOG"

echo "Building done" | tee -a "$BUILD_LOG"
