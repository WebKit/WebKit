#!/bin/bash

set -eo pipefail

# Cleanup old files.
rm -rf build/
rm -rf compose-multiplatform/

BUILD_LOG="$(realpath build.log)"
echo -e "Built on $(date --rfc-3339=seconds)" | tee "$BUILD_LOG"

# Build the benchmark from source.
# FIXME: Use main branch and remove hotfix patch below, once
# https://youtrack.jetbrains.com/issue/SKIKO-1040 is resolved upstream.
# See https://github.com/WebKit/JetStream/pull/84#discussion_r2252418900.
git clone -b ok/jetstream3_hotfix https://github.com/JetBrains/compose-multiplatform.git |& tee -a "$BUILD_LOG"
pushd compose-multiplatform/
git log -1 --oneline | tee -a "$BUILD_LOG"
# Do not read `isD8` in the main function which decides whether to run eagerly.
# Instead, just patch that out statically.
# TODO: Upstream that fix to the compose-multiplatform repo.
git apply ../empty-main-function.patch | tee -a "$BUILD_LOG"
# FIXME: Use stable 2.3 Kotlin/Wasm toolchain, once available around Sep 2025.
git apply ../use-beta-toolchain.patch | tee -a "$BUILD_LOG"
pushd benchmarks/multiplatform
./gradlew :benchmarks:wasmJsProductionExecutableCompileSync
# For building polyfills and JavaScript launcher to run in d8 (which inspires the benchmark.js launcher here):
# ./gradlew :benchmarks:buildD8Distribution
BUILD_SRC_DIR="compose-multiplatform/benchmarks/multiplatform/build/wasm/packages/compose-benchmarks-benchmarks/kotlin"
popd
popd

echo "Copying generated files into build/" | tee -a "$BUILD_LOG"
mkdir -p build/ | tee -a "$BUILD_LOG"
cp $BUILD_SRC_DIR/compose-benchmarks-benchmarks.{wasm,uninstantiated.mjs} build/ | tee -a "$BUILD_LOG"
git apply hook-print.patch | tee -a "$BUILD_LOG"
# FIXME: Remove once the JSC fixes around JSTag have landed in Safari, see
# https://github.com/WebKit/JetStream/pull/84#issuecomment-3164672425.
git apply jstag-workaround.patch | tee -a "$BUILD_LOG"
cp $BUILD_SRC_DIR/skiko.{wasm,mjs} build/ | tee -a "$BUILD_LOG"
git apply skiko-disable-instantiate.patch | tee -a "$BUILD_LOG"
cp $BUILD_SRC_DIR/composeResources/compose_benchmarks.benchmarks.generated.resources/drawable/example1_cat.jpg build/ | tee -a "$BUILD_LOG"
cp $BUILD_SRC_DIR/composeResources/compose_benchmarks.benchmarks.generated.resources/drawable/compose-multiplatform.png build/ | tee -a "$BUILD_LOG"
cp $BUILD_SRC_DIR/composeResources/compose_benchmarks.benchmarks.generated.resources/files/example1_compose-community-primary.png build/ | tee -a "$BUILD_LOG"
cp $BUILD_SRC_DIR/composeResources/compose_benchmarks.benchmarks.generated.resources/font/jetbrainsmono_*.ttf build/ | tee -a "$BUILD_LOG"
echo "Build success" | tee -a "$BUILD_LOG"
