#!/bin/bash

set -eo pipefail

# Cleanup old files.
rm -rf build/
rm -rf j2cl/

BUILD_LOG="$(realpath build.log)"
echo -e "Built on $(date --rfc-3339=seconds)" | tee "$BUILD_LOG"

# Build the benchmark from source.
git clone git@github.com:google/j2cl.git |& tee -a "$BUILD_LOG"
pushd j2cl/
git log -1 --oneline | tee -a "$BUILD_LOG"
git apply ../add-fixed-run-count-api.patch | tee -a "$BUILD_LOG"
BUILD_SRC_DIR="benchmarking/java/com/google/j2cl/benchmarks/octane/"
pushd "$BUILD_SRC_DIR"
bazel build //benchmarking/java/com/google/j2cl/benchmarks/octane:Box2dBenchmark_local-j2wasm-v8
popd
popd

echo "Copying generated files into build/" | tee -a "$BUILD_LOG"
mkdir -p build/ | tee -a "$BUILD_LOG"
BUILD_OUT_DIR="j2cl/bazel-bin/$BUILD_SRC_DIR"
cp $BUILD_OUT_DIR/Box2dBenchmark_j2wasm_entry.js build/ | tee -a "$BUILD_LOG"
# FIXME: Remove this workaround, once Safari/JSC implements JS-string builtins.
# Since these imports/builtins are never called in the workload, they should not have any effect on runtime.
sed -i 's/imports:/"wasm:js-string":{fromCharCodeArray:unused_import,concat:unused_import,equals:unused_import,compare:unused_import,length:unused_import,charCodeAt:unused_import,substring:unused_import},imports:/g' build/Box2dBenchmark_j2wasm_entry.js
cp $BUILD_OUT_DIR/Box2dBenchmark_j2wasm_binary.wasm build/ | tee -a "$BUILD_LOG"
cp $BUILD_OUT_DIR/Box2dBenchmark_j2wasm_binary.binaryen.symbolmap build/ | tee -a "$BUILD_LOG"
echo "Build success" | tee -a "$BUILD_LOG"
