//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 12: 0th type failed to parse because struct types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  LoadEliminationtFreshKnownTest@gc-optimizations.js:65:37
//  global code@gc-optimizations.js:68:3

// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --no-liftoff

// This tests are meant to examine if Turbofan CsaLoadElimination works
// correctly for wasm. The TurboFan graphs can be examined with --trace-turbo.
load("wasm-module-builder.js");

// Fresh objects, known offsets
(function LoadEliminationtFreshKnownTest() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true),
                                  makeField(kWasmI32, true)]);

  builder.addFunction("main", makeSig([kWasmI32], [kWasmI32]))
    .addLocals(wasmRefNullType(struct), 1)
    .addBody([
      kExprI32Const, 10,  // local1 = struct(10, 100);
      kExprI32Const, 100,
      kGCPrefix, kExprStructNew, struct,
      kExprLocalSet, 1,
      kExprLocalGet, 0,  // Split control based on an unknown value
      kExprIf, kWasmI32,
        kExprLocalGet, 1,  // local1.field1 = 42
        kExprI32Const, 42,
        kGCPrefix, kExprStructSet, struct, 1,
        kExprLocalGet, 1,  // local1.field1
        kGCPrefix, kExprStructGet, struct, 1,
      kExprElse,
        kExprLocalGet, 1,  // local1.field1 = 11
        kExprI32Const, 11,
        kGCPrefix, kExprStructSet, struct, 1,
        kExprLocalGet, 1,  // local1.field1 = 22
        kExprI32Const, 22,
        kGCPrefix, kExprStructSet, struct, 1,
        kExprLocalGet, 1,    // local1.field1 + local1.field1
        kGCPrefix, kExprStructGet, struct, 1,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructGet, struct, 1,
        kExprI32Add,
      kExprEnd,
      kExprLocalGet, 1,  // return if-result * (local1.field1 + local1.field0)
      kGCPrefix, kExprStructGet, struct, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructGet, struct, 1,
      kExprI32Add,
      kExprI32Mul
    ])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(instance.exports.main(1), 42 * (42 + 10));
  assertEquals(instance.exports.main(0), (22 + 22) * (22 + 10));
})();

(function LoadEliminationtConstantKnownTest() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  let replaced_value = 55
  let param_1_value = 42
  let init_value_1 = 5
  let init_value_2 = 17

  let tester = builder.addFunction("tester", makeSig(
      [wasmRefType(struct), wasmRefType(struct)], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, struct, 0,

      kExprLocalGet, 0,
      kExprI32Const, replaced_value,
      kGCPrefix, kExprStructSet, struct, 0,

      // We should eliminate this load and replace it with replaced_value
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, struct, 0,

      kExprLocalGet, 1,
      kExprI32Const, param_1_value,
      kGCPrefix, kExprStructSet, struct, 0,

      // Although we could eliminate this load before, we cannot anymore,
      // because the parameters may alias.
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, struct, 0,

      kExprI32Add, kExprI32Add
    ]);

  function buildStruct(value) {
    return [kExprI32Const, value,
            kGCPrefix, kExprStructNew, struct];
  }

  builder.addFunction("main_non_aliasing", kSig_i_v)
    .addBody([
      ...buildStruct(init_value_1), ...buildStruct(init_value_2),
      kExprCallFunction, tester.index])
    .exportFunc();

  builder.addFunction("main_aliasing", kSig_i_v)
    .addLocals(wasmRefNullType(struct), 1)
    .addBody([
      ...buildStruct(init_value_1), kExprLocalSet, 0,
      kExprLocalGet, 0, kExprRefAsNonNull,
      kExprLocalGet, 0, kExprRefAsNonNull,
      kExprCallFunction, tester.index])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(init_value_1 + replaced_value + replaced_value,
               instance.exports.main_non_aliasing());
  assertEquals(init_value_1 + replaced_value + param_1_value,
               instance.exports.main_aliasing());
})();

(function LoadEliminationtArbitraryKnownTest() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  let initial_value = 19;
  let replacing_value_1 = 55;
  let replacing_value_2 = 37;

  let id = builder.addFunction("id", makeSig([wasmRefNullType(struct)],
                                             [wasmRefNullType(struct)]))
      .addBody([kExprLocalGet, 0])

  builder.addFunction("main", kSig_i_v)
    .addLocals(wasmRefNullType(struct), 2)
    .addBody([
      // We store a fresh struct in local0
      kExprI32Const, initial_value,
      kGCPrefix, kExprStructNew, struct,
      kExprLocalSet, 0,

      // We pass it through a function and store it to local1. local1 may now
      // alias with anything.
      kExprLocalGet, 0, kExprCallFunction, id.index, kExprLocalSet, 1,

      kExprLocalGet, 0,
      kExprI32Const, replacing_value_1,
      kGCPrefix, kExprStructSet, struct, 0,

      // We should eliminate this load.
      kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,

      kExprLocalGet, 1,
      kExprI32Const, replacing_value_2,
      kGCPrefix, kExprStructSet, struct, 0,

      // We should not eliminate this load.
      kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0,

      kExprI32Add])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(replacing_value_1 + replacing_value_2, instance.exports.main());
})();

(function LoadEliminationtFreshUnknownTest() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let array = builder.addArray(kWasmI64, true);

  // parameter: unknown array index
  builder.addFunction("main", makeSig([kWasmI32], [kWasmI32]))
    .addLocals(wasmRefNullType(array), 1)
    .addBody([
      kExprI32Const, 5,
      kGCPrefix, kExprArrayNewDefault, array,
      kExprLocalSet, 1,

      kExprLocalGet, 1,  // a[i] = i for i = {0..4}
      kExprI32Const, 0,
      kExprI64Const, 0,
      kGCPrefix, kExprArraySet, array,

      kExprLocalGet, 1,
      kExprI32Const, 1,
      kExprI64Const, 1,
      kGCPrefix, kExprArraySet, array,

      kExprLocalGet, 1,
      kExprI32Const, 2,
      kExprI64Const, 2,
      kGCPrefix, kExprArraySet, array,

      kExprLocalGet, 1,
      kExprI32Const, 3,
      kExprI64Const, 3,
      kGCPrefix, kExprArraySet, array,

      kExprLocalGet, 1,
      kExprI32Const, 4,
      kExprI64Const, 4,
      kGCPrefix, kExprArraySet, array,

      // Get a constant index a[4] before setting unknown indices
      kExprLocalGet, 1,
      kExprI32Const, 4,
      kGCPrefix, kExprArrayGet, array,

      kExprLocalGet, 1,  // Set a[local0] = 33
      kExprLocalGet, 0,
      kExprI64Const, 33,
      kGCPrefix, kExprArraySet, array,

      kExprLocalGet, 1,  // Get a[local0]
      kExprLocalGet, 0,
      kGCPrefix, kExprArrayGet, array,

      kExprLocalGet, 1,  // Known index load cannot be eliminated anymore
      kExprI32Const, 3,
      kGCPrefix, kExprArrayGet, array,

      // A load from different unknown index a[local0 + 1] cannot be eliminated
      kExprLocalGet, 1,
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI32Add,
      kGCPrefix, kExprArrayGet, array,

      kExprI64Add, // return a[4] * (a[local0] - (a[3] + a[local0 + 1]))
      kExprI64Sub,
      kExprI64Mul,
      kExprI32ConvertI64  // To not have to worry about BigInts in JS world
    ])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(4 * (33 - (3 + 1)), instance.exports.main(0));
  assertEquals(4 * (33 - (3 + 2)), instance.exports.main(1));
  assertEquals(4 * (33 - (3 + 3)), instance.exports.main(2));
  assertEquals(4 * (33 - (33 + 4)), instance.exports.main(3));
})();

(function LoadEliminationtAllBetsAreOffTest() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let array = builder.addArray(kWasmI32, true);

  let value_0 = 19;
  let value_1 = 55;
  let value_2 = 2;

  let id = builder.addFunction("id", makeSig([wasmRefNullType(array)],
                                             [wasmRefNullType(array)]))
      .addBody([kExprLocalGet, 0])

  // parameters: array, index
  let tester = builder.addFunction("tester",
      makeSig([wasmRefType(array), kWasmI32], [kWasmI32]))
    .addLocals(wasmRefNullType(struct), 1)
    .addLocals(wasmRefNullType(array), 1)
    .addBody([
      // We store a fresh struct in local1
      kExprI32Const, 0,
      kGCPrefix, kExprStructNew, struct,
      kExprLocalSet, 2,

      // We pass the array parameter through a function and store it to local2.
      kExprLocalGet, 0, kExprCallFunction, id.index, kExprLocalSet, 3,

      // Set the parameter array, the fresh struct, then the arbitrary array to
      // an unknown offset.
      kExprLocalGet, 0,
      kExprI32Const, 5,
      kExprI32Const, value_0,
      kGCPrefix, kExprArraySet, array,

      kExprLocalGet, 2,
      kExprI32Const, value_1,
      kGCPrefix, kExprStructSet, struct, 0,

      kExprLocalGet, 3,
      kExprLocalGet, 1,
      kExprI32Const, value_2,
      kGCPrefix, kExprArraySet, array,

      // Neither load can be eliminated.
      kExprLocalGet, 0,
      kExprI32Const, 5,
      kGCPrefix, kExprArrayGet, array,

      kExprLocalGet, 2,
      kGCPrefix, kExprStructGet, struct, 0,

      kExprI32Add]);

  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprI32Const, 10,
      kGCPrefix, kExprArrayNewDefault, array,
      kExprI32Const, 7,
      kExprCallFunction, tester.index,
    ])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(value_0 + value_1, instance.exports.main());
})();

(function EscapeAnalysisWithLoadElimination() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct1 = builder.addStruct([makeField(kWasmI32, true)]);
  let struct2 = builder.addStruct([makeField(wasmRefNullType(struct1), true)]);

  // TF should eliminate both allocations in this function.
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct1,
      kGCPrefix, kExprStructNew, struct2,
      kGCPrefix, kExprStructGet, struct2, 0,
      kGCPrefix, kExprStructGet, struct1, 0])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(42, instance.exports.main(42));
})();

(function EscapeAnalysisWithInterveningEffect() {
  // print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct1 = builder.addStruct([makeField(kWasmI32, true)]);
  let struct2 = builder.addStruct([makeField(wasmRefNullType(struct1), true)]);

  let nop = builder.addFunction("nop", kSig_v_v).addBody([]);

  // TF should eliminate both allocations in this function, despite the
  // intervening effectful call.
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct1,
      kExprCallFunction, nop.index,
      kGCPrefix, kExprStructNew, struct2,
      kExprLocalGet, 0,
      kExprReturn])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(42, instance.exports.main(42));
})();

(function AllocationFolding() {
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  let struct_index = builder.addStruct([makeField(kWasmI32, true)]);
  let struct_2 = builder.addStruct([
    makeField(wasmRefType(struct_index), false),
    makeField(wasmRefType(struct_index), false)
  ]);

  let global = builder.addGlobal(
      wasmRefNullType(struct_2), true, [kExprRefNull, struct_2]);

  // The three alocations should be folded.
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct_index,
      kExprI32Const, 43,
      kGCPrefix, kExprStructNew, struct_index,
      kGCPrefix, kExprStructNew, struct_2,
      kExprGlobalSet, global.index,
      kExprLocalGet, 0,
    ])
    .exportFunc();

  let instance = builder.instantiate();
  assertEquals(10, instance.exports.main(10));
})();

(function PathBasedTypedOptimization() {
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  let super_struct = builder.addStruct([makeField(kWasmI32, true)]);
  let mid_struct = builder.addStruct(
    [makeField(kWasmI32, true), makeField(kWasmI32, true)], super_struct);
  let sub_struct = builder.addStruct(
    [makeField(kWasmI32, true), makeField(kWasmI32, true),
     makeField(kWasmI32, true)],
    mid_struct);

  let addToLocal = [kExprLocalGet, 1, kExprI32Add, kExprLocalSet, 1];

  builder.addFunction(
      "main", makeSig([wasmRefNullType(super_struct)], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTestDeprecated, sub_struct,

      // These casts have to be preserved.
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, mid_struct,
      kGCPrefix, kExprRefCast, sub_struct,
      kGCPrefix, kExprStructGet, sub_struct, 1,
      ...addToLocal,

      kExprIf, kWasmVoid,
        // Both these casts should be optimized away.
        kExprLocalGet, 0,
        kGCPrefix, kExprRefCast, mid_struct,
        kGCPrefix, kExprRefCast, sub_struct,
        kGCPrefix, kExprStructGet, sub_struct, 1,
        ...addToLocal,

        kExprBlock, kWasmRefNull, super_struct,
          kExprLocalGet, 0,
          // This should also get optimized away.
          kGCPrefix, kExprBrOnCastFail, 0, mid_struct,
          // So should this, despite being represented by a TypeGuard alias.
          kGCPrefix, kExprRefCast, sub_struct,
          kGCPrefix, kExprStructGet, sub_struct, 1,
          ...addToLocal,
          kExprLocalGet, 0,  // Due to the branch result type.
        kExprEnd,
        kExprDrop,
      kExprElse,
        // This (always trapping) cast should be preserved.
        kExprLocalGet, 0,
        kGCPrefix, kExprRefCast, sub_struct,
        kGCPrefix, kExprStructGet, sub_struct, 1,
        ...addToLocal,
      kExprEnd,
      // This cast should be preserved.
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, sub_struct,
      kGCPrefix, kExprStructGet, sub_struct, 1,
      kExprLocalGet, 1, kExprI32Add
    ])
    .exportFunc();

  builder.instantiate();
})();

(function IndependentCastNullRefType() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_super = builder.addStruct([makeField(kWasmI32, true)]);
  let struct_b = builder.addStruct([makeField(kWasmI32, true)], struct_super);
  let struct_a = builder.addStruct(
    [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct_super);

  let callee_sig = makeSig([wasmRefNullType(struct_a)], [kWasmI32]);
  let callee = builder.addFunction("callee", callee_sig)
    .addBody([
      // Cast from struct_a to struct_b via common base type struct_super.
      kExprLocalGet, 0,
      // TODO(7748): Replace cast op with "ref.cast null".
      kGCPrefix, kExprRefCastDeprecated, struct_super,
      kGCPrefix, kExprRefCastDeprecated, struct_b, // annotated as 'ref null none'
      kExprRefIsNull,
    ]);

  builder.addFunction("main", kSig_i_i)
    .addLocals(wasmRefNullType(struct_a), 1)
    .addBody([
      kExprLocalGet, 0,
      kExprIf, kWasmVoid,
      kExprI32Const, 10,
      kExprI32Const, 100,
      kGCPrefix, kExprStructNew, struct_a,
      kExprLocalSet, 1,
      kExprEnd,
      kExprLocalGet, 1,
      kExprCallFunction, callee.index
    ]).exportFunc();

  let instance = builder.instantiate({});
  // main calls 'callee(null)'
  // -> (ref.is_null (ref.cast struct_b (ref.cast struct_super (local.get 0))))
  //    returns true.
  assertEquals(1, instance.exports.main(0));
  // main calls 'callee(struct.new struct_a)'
  // -> (ref.cast struct_b) traps.
  assertTraps(kTrapIllegalCast, () => instance.exports.main(1));
})();

(function StaticCastOfKnownNull() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_super = builder.addStruct([makeField(kWasmI32, true)]);
  let struct_b = builder.addStruct([makeField(kWasmI32, true)], struct_super);
  let struct_a = builder.addStruct(
    [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct_super);

  let callee_sig = makeSig([wasmRefNullType(struct_super)], [kWasmI32]);
  let callee = builder.addFunction("callee", callee_sig)
    .addBody([
      kExprBlock, kWasmRefNull, struct_super,
      kExprLocalGet, 0,
      kExprBrOnNonNull, 0,
      // local.get 0 is known to be null until end of block.
      kExprLocalGet, 0,
      // This cast is a no-op and shold be optimized away.
      // TODO(7748): Replace with "ref.cast null".
      kGCPrefix, kExprRefCastDeprecated, struct_b,
      kExprEnd,
      kExprRefIsNull,
    ]);

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprRefNull, struct_a,
      kExprCallFunction, callee.index
    ]).exportFunc();

  let instance = builder.instantiate({});
  assertEquals(1, instance.exports.main());
})();

(function AssertNullAfterCastIncompatibleTypes() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_super = builder.addStruct([makeField(kWasmI32, true)]);
  let struct_b = builder.addStruct([makeField(kWasmI32, true)], struct_super);
  let struct_a = builder.addStruct(
    [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct_super);
  let callee_sig = makeSig([wasmRefNullType(struct_super)], [kWasmI32]);

  builder.addFunction("mkStruct", makeSig([], [kWasmExternRef]))
    .addBody([kGCPrefix, kExprStructNewDefault, struct_a,
              kGCPrefix, kExprExternExternalize])
    .exportFunc();

  let callee = builder.addFunction("callee", callee_sig)
    .addBody([
       kExprLocalGet, 0, kGCPrefix, kExprRefCast, struct_b,
       kExprRefAsNonNull,
       kGCPrefix, kExprStructGet, struct_b, 0]);

  builder.addFunction("main", makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprExternInternalize,
              kGCPrefix, kExprRefAsStruct,
              kGCPrefix, kExprRefCast, struct_a,
              kExprCallFunction, callee.index])
    .exportFunc();

  let instance = builder.instantiate({});
  assertTraps(kTrapIllegalCast,
              () => instance.exports.main(instance.exports.mkStruct()));
})();

(function StructGetMultipleNullChecks() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true),
                                  makeField(kWasmI32, true)]);

  builder.addFunction("main",
                      makeSig([kWasmI32, wasmRefNullType(struct)], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprIf, kWasmI32,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprLocalGet, 1,
        // The null check should be removed for this struct.
        kGCPrefix, kExprStructGet, struct, 1,
        kExprI32Add,
      kExprElse,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructGet, struct, 0,
      kExprEnd,
      kExprLocalGet, 1,
      // The null check here could be removed if we compute type intersections.
      kGCPrefix, kExprStructGet, struct, 1,
      kExprI32Mul])
    .exportFunc();

  builder.instantiate({});
})();
