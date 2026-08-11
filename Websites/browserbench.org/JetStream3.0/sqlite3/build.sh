#!/bin/bash

set -eo pipefail

# Cleanup old files.
rm -f sqlite-src-*.zip
rm -rf sqlite-src-*/
rm -rf build/

touch build.log
BUILD_LOG="$(realpath build.log)"
echo -e "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')\n" | tee "$BUILD_LOG"

echo "Toolchain versions" | tee -a "$BUILD_LOG"
emcc --version | head -n1 | tee -a "$BUILD_LOG"
echo -e "wasm-strip $(wasm-strip --version)\n" | tee -a "$BUILD_LOG"

# Check https://sqlite.org/download.html and update the source link, if needed.
SQLITE_SRC_URL="https://sqlite.org/2025/sqlite-src-3480000.zip"
echo -e "Getting sources from $SQLITE_SRC_URL\n" | tee -a "$BUILD_LOG"
SQLITE_SRC_FILE="$(basename $SQLITE_SRC_URL)"
curl -o "$SQLITE_SRC_FILE" $SQLITE_SRC_URL
unzip "$SQLITE_SRC_FILE"

# Paths and information in make output could be sensitive, so don't save in log.
echo "Building..." | tee -a "$BUILD_LOG"
pushd sqlite-src-*/
./configure
cd ext/wasm
make dist
popd

echo "Copying files from sqlite-src-*/ext/wasm/ into build/" | tee -a "$BUILD_LOG"
mkdir -p build/{common,jswasm} | tee -a "$BUILD_LOG"
cp sqlite-src-*/ext/wasm/jswasm/speedtest1.{js,wasm} build/jswasm/ | tee -a "$BUILD_LOG"
# The next files are only needed for the upstream browser build, not the
# JetStream version, hence don't copy them by default.
# cp sqlite-src-*/ext/wasm/speedtest1.html build/ | tee -a "$BUILD_LOG"
# cp sqlite-src-*/ext/wasm/common/{emscripten.css,SqliteTestUtil.js,testing.css} build/common/ | tee -a "$BUILD_LOG"

echo "Build success" | tee -a "$BUILD_LOG"
