#!/bin/bash

# This script is used to compile Swift WASM.
# Note that the corresponding tests needs to be updated due to updated WASM binary.
set -e

cd "$(dirname "$0")"

# Build Swift WASM with Swiftly https://www.swift.org/documentation/articles/wasm-getting-started.html
echo "Building Swift WebAssembly..."
/Users/yijiahuang/.swiftly/bin/swift build --swift-sdk swift-6.2.3-RELEASE_wasm

# Move compiled WASM beside main.js
echo "Moving test.wasm..."
mv .build/wasm32-unknown-wasip1/debug/test.wasm ./test.wasm

# Clean up build artifacts
echo "Cleaning up..."
rm -rf .build

echo "Done!"
