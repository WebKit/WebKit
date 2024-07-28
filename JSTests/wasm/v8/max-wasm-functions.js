//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 23: Function section's count is too big 1000010 maximum 1000000 (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  global code@max-wasm-functions.js:25:37

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-wasm-functions=1000100

load("wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig([kWasmI32], [kWasmI32]));

for (let j = 0; j < 1000010; ++j) {
  builder.addFunction(undefined, sig_index)
    .addBody([kExprLocalGet, 0]);
}
const instance = builder.instantiate();
