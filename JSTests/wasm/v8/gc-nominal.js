//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 12: 0th type failed to parse because struct types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  TestNominalTypesBasic@gc-nominal.js:45:22
//  global code@gc-nominal.js:46:3

// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

load("wasm-module-builder.js");

(function TestNominalTypesBasic() {
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  let struct1 = builder.addStruct([makeField(kWasmI32, true)]);
  let struct2 = builder.addStruct(
      [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct1);

  let array1 = builder.addArray(kWasmI32, true);
  let array2 = builder.addArray(kWasmI32, true, array1);

  builder.addFunction("main", kSig_v_v)
      .addLocals(wasmRefNullType(struct1), 1)
      .addLocals(wasmRefNullType(array1), 1)
      .addBody([
        // Check that we can create a struct with implicit RTT.
        kGCPrefix, kExprStructNewDefault, struct2,
        // ...and upcast it.
        kExprLocalSet, 0,
        // Check that we can create an array with implicit RTT.
        kExprI32Const, 10,  // length
        kGCPrefix, kExprArrayNewDefault, array2,
        // ...and upcast it.
        kExprLocalSet, 1])
      .exportFunc();

  // This test is only interested in type checking.
  builder.instantiate();
})();

(function TestSubtypingDepthTooLarge() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addStruct([]);
  for (let i = 0; i < 32; i++) builder.addStruct([], i);
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      /subtyping depth is greater than allowed/);
})();
