//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 12: 0th type failed to parse because rec types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  WasmTypedOptimizationsTest@type-based-optimizations.js:79:22
//  global code@type-based-optimizations.js:80:3

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --no-liftoff

load("wasm-module-builder.js");

// Test that we can eliminate type checks based on narrowed argument types
// (by inspecting the resulting graph).
(function WasmTypedOptimizationsTest() {
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  let top = builder.addStruct([makeField(kWasmI32, true)]);
  let middle = builder.addStruct([makeField(kWasmI32, true),
                                  makeField(kWasmI64, false)],
                                 top);
  let bottom1 = builder.addStruct([makeField(kWasmI32, true),
                                   makeField(kWasmI64, false),
                                   makeField(kWasmI32, true)],
                                  middle);
  let bottom2 = builder.addStruct([makeField(kWasmI32, true),
                                   makeField(kWasmI64, false),
                                   makeField(kWasmI64, false)],
                                  middle);
  builder.endRecGroup();

  builder.addFunction("main", makeSig(
        [wasmRefType(bottom1), wasmRefType(bottom2)], [kWasmI32]))
    .addLocals(wasmRefNullType(top), 1)
    .addLocals(kWasmI32, 1)
    .addBody([
        // temp = x0;
        kExprLocalGet, 0, kExprLocalSet, 2,
        // while (true) {
        kExprLoop, kWasmVoid,
          // if (ref.test temp bottom1) {
          kExprLocalGet, 2, kGCPrefix, kExprRefTestDeprecated, bottom1,
          kExprIf, kWasmVoid,
            // counter += ((bottom1) temp).field_2;
            // Note: This cast should get optimized away with path-based type
            // tracking.
            kExprLocalGet, 2, kGCPrefix, kExprRefCast, bottom1,
            kGCPrefix, kExprStructGet, bottom1, 2,
            kExprLocalGet, 3, kExprI32Add, kExprLocalSet, 3,
            // temp = x1;
            kExprLocalGet, 1, kExprLocalSet, 2,
          // } else {
          kExprElse,
            // counter += (i32) ((middle) temp).field_1
            // Note: This cast should get optimized away, as temp only gets
            // assigned to {bottom1} and {bottom2}.
            kExprLocalGet, 2, kGCPrefix, kExprRefCast, middle,
            kGCPrefix, kExprStructGet, middle, 1, kExprI32ConvertI64,
            kExprLocalGet, 3, kExprI32Add, kExprLocalSet, 3,
            // temp = x0;
            kExprLocalGet, 0, kExprLocalSet, 2,
          // }
          kExprEnd,
          // if (counter < 100) continue; break;
          kExprLocalGet, 3, kExprI32Const, 100, kExprI32LtS,
          kExprBrIf, 0,
        // }
        kExprEnd,
        // return counter;
        kExprLocalGet, 3])
    .exportFunc();

  builder.instantiate({});
})();
