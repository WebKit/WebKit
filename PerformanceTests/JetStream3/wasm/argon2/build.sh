#!/usr/bin/env bash

set -e
set -o pipefail

# Log emcc version
EMCC_SDK_PATH="/path/to/emsdk"
EMCC_PATH="$EMCC_SDK_PATH/upstream/emscripten/emcc"
$EMCC_PATH --version > emcc_version.txt

# Build start
rm -rf dist
mkdir dist

./clean-cmake.sh
EMCC_SDK_PATH=$EMCC_SDK_PATH ARGON_JS_BUILD_BUILD_WITH_SIMD=1 ./build-wasm.sh
mv dist/argon2.wasm ../argon2-simd.wasm

./clean-cmake.sh
EMCC_SDK_PATH=$EMCC_SDK_PATH ARGON_JS_BUILD_BUILD_WITH_SIMD=0 ./build-wasm.sh
mv dist/argon2.wasm ../argon2.wasm

./clean-cmake.sh
rm -rf dist
# Build end

echo Done
