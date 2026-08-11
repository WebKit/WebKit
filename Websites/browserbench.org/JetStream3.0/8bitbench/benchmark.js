// Copyright 2025 the V8 project authors. All rights reserved.
// Copyright 2025 Apple Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simplified from `8bitbench/cli.mjs` and `8bitbench/js/bench.js`.
// See also `8bitbench/rust/src/main.rs` for the native version.
function log(str) { print(str); }
function getInput() { return "t"; }
globalThis.updateVideo = function(vec) { }

const frameWidth = 256;
const frameHeight = 240;
const channels = 4;

// For debugging only: Dump a rendered frame (as a linear vector of pixels) to
// to an ASCII-encoded PPM image file.
function dumpFrame(vec) {
  const maxValue = 255;
  let ppm = `P3\n${frameWidth} ${frameHeight}\n${maxValue}\n`
  for (let y = 0; y < frameHeight; ++y) {
    for (let x = 0; x < frameWidth; ++x) {
      const r = vec[x*channels + y*frameWidth*channels];
      const g = vec[1 + x*channels + y*frameWidth*channels];
      const b = vec[2 + x*channels + y*frameWidth*channels];
      const alpha = vec[3 + x*channels + y*frameWidth*channels];
      ppm += `${r} ${g} ${b}\n`
      if (alpha !== maxValue) {
        throw new Error("Unexpected alpha channel value: " + alpha);
      }
    }
  }
  writeFile('last_frame.ppm', ppm);
}

class Benchmark {
  isInstantiated = false;
  romBinary;

  async init() {
    Module.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
    this.romBinary = await JetStream.getBinary(JetStream.preload.romBinary);
  }

  async runIteration() {
    if (!this.isInstantiated) {
      await wasm_bindgen(Module.wasmBinary);
      this.isInstantiated = true;
    }

    wasm_bindgen.loadRom(this.romBinary);

    const frameCount = 2 * 60;
    for (let i = 0; i < frameCount; ++i) {
      wasm_bindgen.js_tick();
    }
  }

  validate() {
    globalThis.updateVideo = function(vec) {
      if (vec.length != frameWidth * frameHeight * channels) {
        throw new Error("Wrong video vec length");
      }
      // dumpFrame(vec);

      let videoSum = 0;
      for (let i = 0; i < vec.length; ++i) {
        videoSum += vec[i]
      }
      if (videoSum != 40792276) {
        throw new Error("Wrong video sum, the picture is wrong: " + videoSum);
      }
    }

    wasm_bindgen.js_tick();
  }
}
