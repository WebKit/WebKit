#!/usr/bin/env bash

set -euo pipefail

rm -rf build/

touch build.log
BUILD_LOG="$(realpath build.log)"
echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee "$BUILD_LOG"

echo "Toolchain versions" | tee -a "$BUILD_LOG"
emcc --version | head -n1 | tee -a "$BUILD_LOG"

# FIXME: Redownload the source (from https://github.com/P-H-C/phc-winner-argon2) if argon2 ever has source updates. At the time of writing it was last changed 5 years ago so this is probably not a high priority.
SOURCES=(
    src/blake2/blake2b.c

    src/argon2.c
    src/core.c
    src/encoding.c
    src/thread.c

    src/opt.c
)

SIMD_FLAGS=(
    -msimd128
    -msse2
)

echo "Building..." | tee -a "$BUILD_LOG"
mkdir build/
emcc -o build/argon2.js \
    -s WASM=1 -O2 \
    ${SIMD_FLAGS[@]} \
    -g1 --emit-symbol-map \
    -DARGON2_NO_THREADS \
    -s MODULARIZE=1 -s EXPORT_NAME=setupModule -s EXPORTED_RUNTIME_METHODS=stringToNewUTF8,UTF8ToString -s EXPORTED_FUNCTIONS=_argon2_hash,_argon2_verify,_argon2_encodedlen,_argon2_error_message,_malloc,_free,_strlen \
    -Iinclude \
    ${SOURCES[@]} | tee -a "$BUILD_LOG"

node ../../utils/compress.mjs build/argon2.wasm

echo "Building done" | tee -a "$BUILD_LOG"
ls -lth build/
