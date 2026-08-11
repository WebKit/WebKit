#!/bin/bash

set -euo pipefail

rm -rf build/
mkdir -p build/{models,inputs,onnxruntime-web}/

# Optional: clean all node packages as well.
rm -rf util/node_modules/

touch build.log
BUILD_LOG="$(realpath build.log)"
echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee "$BUILD_LOG"

echo "Installing Node dependencies..." | tee -a "$BUILD_LOG"
pushd util/
npm install
popd

echo "Download and convert audio input(s)..." | tee -a "$BUILD_LOG"
wget https://huggingface.co/datasets/Xenova/transformers.js-docs/resolve/main/jfk.wav | tee -a "$BUILD_LOG"
# Shorten the audio file to one sentence in the middle, to speed up a single iteration.
node util/convert-audio.mjs jfk.wav build/inputs/jfk.raw 52000 120000 | tee -a "$BUILD_LOG"
rm jfk.wav

echo "Download and run model(s)..." | tee -a "$BUILD_LOG"
# This automatically places the model files in `build/models/`.
node util/test-models.mjs

echo "Copy library files into build/..." | tee -a "$BUILD_LOG"

cp util/node_modules/@huggingface/transformers/dist/transformers.js build/
git apply transformers.js.patch

# Transformers.js packages the ONNX runtime JSEP build by default, even when
# only using the Wasm backend, which would be fine with the non-JSEP build.
# JSEP uses ASYNCIFY, which isn't optimal. And it's a much larger Wasm binary.
# cp util/node_modules/@huggingface/transformers/dist/ort-wasm-simd-threaded.jsep.{mjs,wasm} build/

# There is also an ONNX runtime build in the onnxruntime-web package.
# TODO(dlehmann): Discuss with upstream Transformers.js folks, whether they can
# use the non-JSEP build if one requests the Wasm backend.
# TODO(dlehmann): Measure performance difference between the two.
cp util/node_modules/onnxruntime-web/dist/ort-wasm-simd-threaded.{mjs,wasm} build/onnxruntime-web/

# TODO: Compress model data (and maybe Wasm modules) with zstd.
# Either decompress with native APIs available in browsers or JS/Wasm polyfill?
# E.g., https://github.com/101arrowz/fzstd or https://github.com/fabiospampinato/zstandard-wasm or https://github.com/donmccurdy/zstddec-wasm

echo "Building done" | tee -a "$BUILD_LOG"
