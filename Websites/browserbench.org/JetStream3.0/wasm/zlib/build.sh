#!/bin/bash

set -euo pipefail

rm -rf build/
rm -f src/zpipe.c

touch build.log
BUILD_LOG="$(realpath build.log)"
echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee "$BUILD_LOG"

echo "Toolchain versions" | tee -a "$BUILD_LOG"
emcc --version | head -n1 | tee -a "$BUILD_LOG"

echo "Getting zpipe.c example source..." | tee -a "$BUILD_LOG"
curl -o "src/zpipe.c" https://www.zlib.net/zpipe.c

echo "Building..." | tee -a "$BUILD_LOG"
mkdir build/
emcc -o build/zlib.js \
    -s WASM=1 -O2 \
    -g1 --emit-symbol-map \
    -s USE_ZLIB=1 -s FORCE_FILESYSTEM=1 \
    -s MODULARIZE=1 -s EXPORT_NAME=setupModule -s EXPORTED_RUNTIME_METHODS=FS,stringToNewUTF8 -s EXPORTED_FUNCTIONS=_compressFile,_decompressFile,_free \
    src/zpipe.c src/main.c | tee -a "$BUILD_LOG"
# If you want the native build for reference:
# clang -o build/zlib -O2 -lz src/zpipe.c src/main.c

echo "Building done" | tee -a "$BUILD_LOG"
ls -lth build/
