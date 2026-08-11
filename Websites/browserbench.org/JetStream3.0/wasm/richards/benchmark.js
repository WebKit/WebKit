// Copyright 2025 the V8 project authors. All rights reserved.
// Copyright 2025 Apple Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Benchmark {
  async init() {
    Module.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
  }

  async runIteration() {
    // Instantiate the Wasm module before the first run.
    if (!Module._setup)
      await setupModule(Module);

    // Set-up the problem (fill the work queue) on each run.
    const taskCount = 200_000;
    Module._setup(taskCount);

    // Repeatedly call into Wasm to stress test JS-to-Wasm call performance.
    // I (dlehmann) suppose this wrapper was added (originally in JetStream 2)
    // to test that a wrapping JavaScript function doesn't slow things down.
    function scheduleIter() {
      return Module._scheduleIter();
    }
    while (scheduleIter()) {}
  }

  validate() {
    if (Module._getQpktcount() !== 465212 || Module._getHoldcount() !== 186084)
      throw new Error("Bad richards result!");
  }
}
