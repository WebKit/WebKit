//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failed because Exception: ReferenceError: Can't find variable: Worker

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("wasm-module-builder.js");

(function TestPostModule() {
  let builder = new WasmModuleBuilder();
  builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
    .exportFunc();

  let module = builder.toModule();

  function workerCode() {
    onmessage = function(module) {
      try {
        let instance = new WebAssembly.Instance(module);
        let result = instance.exports.add(40, 2);
        postMessage(result);
      } catch(e) {
        postMessage('ERROR: ' + e);
      }
    }
  }

  let worker = new Worker(workerCode, {type: 'function'});
  worker.postMessage(module);
  assertEquals(42, worker.getMessage());
  worker.terminate();
})();
