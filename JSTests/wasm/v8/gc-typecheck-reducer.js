//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 12: 0th type failed to parse because struct types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  TestRefTestNonTrivialTypeCheckInlinedTrivial@gc-typecheck-reducer.js:47:37
//  global code@gc-typecheck-reducer.js:55:3

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --no-liftoff

load("wasm-module-builder.js");

// Test inlining of a non-trivial type check (i.e. the decoder can't remove it
// directly) that becomes trivial after inlining.
// This covers a bug in the optimizing compiler treating null as a test failure
// for the "ref.test null" instruction.
(function TestRefTestNonTrivialTypeCheckInlinedTrivial() {
  var builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);


  let refTestFromAny =  builder.addFunction(`refTestFromAny`,
                        makeSig([kWasmAnyRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, struct,
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTestNull, struct,
    ]);

  builder.addFunction(`main`,
      makeSig([], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
    .addBody([
      kExprI32Const, 1,
      kGCPrefix, kExprStructNew, struct,
      kExprCallFunction, refTestFromAny.index,
      kExprRefNull, kNullRefCode,
      kExprCallFunction, refTestFromAny.index,
    ]).exportFunc();

  var instance = builder.instantiate();
  let expected = [
    1,  // ref.test <struct> (struct)
    1,  // ref.test null <struct> (struct)
    0,  // ref.test <struct> (null)
    1   // ref.test null <struct> (null)
  ]
  assertEquals(expected, instance.exports.main());
})();
