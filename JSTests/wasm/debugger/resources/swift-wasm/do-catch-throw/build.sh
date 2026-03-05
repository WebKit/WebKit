#!/bin/bash

# This script is used to compile Swift WASM.
# Note that the corresponding tests needs to be updated due to updated WASM binary.
set -e

cd "$(dirname "$0")"

# Build Swift WASM with Swiftly https://www.swift.org/documentation/articles/wasm-getting-started.html
echo "Building Swift WebAssembly do-catch-throw test..."
/Users/yijiahuang/.swiftly/bin/swift build --swift-sdk swift-6.2.3-RELEASE_wasm

# Move compiled WASM beside main.js
echo "Moving do-catch-throw.wasm..."
mv .build/wasm32-unknown-wasip1/debug/do-catch-throw.wasm ./do-catch-throw.wasm

# Clean up build artifacts
echo "Cleaning up..."
rm -rf .build

echo "Done! do-catch-throw.wasm is ready for testing."
