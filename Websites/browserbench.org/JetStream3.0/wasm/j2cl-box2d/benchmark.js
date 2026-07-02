// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hook into the J2Cl application runner, enables explicit instantiation via `instantiateAsync` below.
const isJetStreamDriver = true;

// Polyfills for shells. See j2cl/bazel-j2cl/benchmarking/java/com/google/j2cl/benchmarking/templates.bzl
class TextDecoder {
  decode(buffer) {
    return String.fromCharCode.apply(null, new Uint8Array(buffer));
  }
}
// The following polyfills are just required for imports but not actually used.
function unused_import() {
  throw new Error('not supported, should be an unused import');
}
var atob = unused_import;
var btoa = unused_import;
var gc = unused_import;

class Benchmark {
  wasmBinary;
  wasmInstanceExports;

  async init() {
    this.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
  }

  async runIteration() {
    // Compile once in the first iteration.`
    if (!this.wasmInstanceExports) {
      this.wasmInstanceExports = (await instantiateAsync(this.wasmBinary)).exports;
    }

    const internalIterations = 5;
    this.wasmInstanceExports.runFixedCount(internalIterations);
  }
}
