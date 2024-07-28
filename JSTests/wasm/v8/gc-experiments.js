//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 12: 0th type failed to parse because struct types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  TestRefCastNop@gc-experiments.js:34:37
//  global code@gc-experiments.js:36:3

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --experimental-wasm-ref-cast-nop

load("wasm-module-builder.js");

(function TestRefCastNop() {
  var builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  builder.addFunction("main", kSig_i_i)
    .addLocals(wasmRefNullType(kWasmStructRef), 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct,
      kExprLocalSet, 1,
      kExprLocalGet, 1,
      kGCPrefix, kExprRefCastNop, struct,
      kGCPrefix, kExprStructGet, struct, 0,
  ]).exportFunc();

  var instance = builder.instantiate();
  assertEquals(42, instance.exports.main(42));
})();
