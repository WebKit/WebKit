#!/bin/bash

set -euo pipefail

touch build.log
BUILD_LOG="$(realpath build.log)"
echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee "$BUILD_LOG"

echo "Toolchain versions" | tee -a "$BUILD_LOG"
em++ --version | head -n1 | tee -a "$BUILD_LOG"

echo "Building..." | tee -a "$BUILD_LOG"
mkdir -p build
em++ -o build/HashSet.js \
    -std=c++11 \
    -s WASM=1 -O2 \
    -s MODULARIZE=1 -s EXPORT_NAME=setupModule \
    -g1 --emit-symbol-map \
    -s TOTAL_MEMORY=52428800 \
    HashSet.cpp | tee -a "$BUILD_LOG"

echo "Building done" | tee -a "$BUILD_LOG"
