//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 13: 0th type failed to parse because rec types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  RefCastFromNull@gc-casts-subtypes.js:84:37
//  global code@gc-casts-subtypes.js:92:3

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --experimental-wasm-type-reflection
// Flags: --no-wasm-gc-structref-as-dataref

load("wasm-module-builder.js");

// Test casting null from one type to another using ref.test & ref.cast.
(function RefCastFromNull() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)]);
  builder.endRecGroup();
  builder.startRecGroup();
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper);
  builder.endRecGroup();
  builder.startRecGroup();
  let array = builder.addArray(kWasmI32);
  builder.endRecGroup();

  // Note: Casting between unrelated types is allowed as long as the types
  // belong to the same type hierarchy (func / any / extern). In these cases the
  // check will always fail.
  let tests = [
    [kWasmAnyRef, kWasmAnyRef, 'AnyToAny'],
    [kWasmFuncRef, kWasmFuncRef, 'FuncToFunc'],
    [kWasmExternRef, kWasmExternRef, 'ExternToExtern'],
    [kWasmNullFuncRef, kWasmNullFuncRef, 'NullFuncToNullFunc'],
    [kWasmNullExternRef, kWasmNullExternRef, 'NullExternToNullExtern'],
    [structSub, array, 'StructToArray'],
    [kWasmFuncRef, kWasmNullFuncRef, 'FuncToNullFunc'],
    [kWasmNullFuncRef, kWasmFuncRef, 'NullFuncToFunc'],
    [kWasmExternRef, kWasmNullExternRef, 'ExternToNullExtern'],
    [kWasmNullExternRef, kWasmExternRef, 'NullExternToExtern'],
    [kWasmNullRef, kWasmAnyRef, 'NullToAny'],
    [kWasmI31Ref, structSub, 'I31ToStruct'],
    [kWasmEqRef, kWasmI31Ref, 'EqToI31'],
    [structSuper, structSub, 'StructSuperToStructSub'],
    [structSub, structSuper, 'StructSubToStructSuper'],
  ];

  for (let [sourceType, targetType, testName] of tests) {
    builder.addFunction('testNull' + testName, makeSig([], [kWasmI32]))
    .addLocals(wasmRefNullType(sourceType), 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefTest, targetType & kLeb128Mask,
    ]).exportFunc();
    builder.addFunction('castNull' + testName, makeSig([], []))
    .addLocals(wasmRefNullType(sourceType), 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, targetType & kLeb128Mask,
      kExprDrop,
    ]).exportFunc();
    builder.addFunction('branchNull' + testName, makeSig([], [kWasmI32]))
    .addLocals(wasmRefNullType(sourceType), 1)
    .addBody([
      kExprBlock, kWasmRef, targetType & kLeb128Mask,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCast, 0, targetType & kLeb128Mask,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();
  }

  let instance = builder.instantiate();
  let wasm = instance.exports;

  for (let [sourceType, targetType, testName] of tests) {
    assertEquals(0, wasm['testNull' + testName]());
    assertTraps(kTrapIllegalCast, wasm['castNull' + testName]);
    assertEquals(0, wasm['branchNull' + testName]());
  }
})();

(function RefTestFuncRef() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sigSuper = builder.addType(makeSig([kWasmI32], []));
  let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

  builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
  builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
  builder.addFunction('testFromFuncRef',
      makeSig([kWasmFuncRef], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kFuncRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kNullFuncRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, sigSuper,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, sigSub,
    ]).exportFunc();
  builder.addFunction('testNullFromFuncRef',
      makeSig([kWasmFuncRef], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kFuncRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kNullFuncRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, sigSuper,
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, sigSub,
    ]).exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let jsFct = new WebAssembly.Function(
      {parameters:['i32', 'i32'], results: ['i32']},
      function mul(a, b) { return a * b; });
  assertEquals([0, 0, 0, 0], wasm.testFromFuncRef(null));
  assertEquals([1, 0, 0, 0], wasm.testFromFuncRef(jsFct));
  assertEquals([1, 0, 1, 0], wasm.testFromFuncRef(wasm.fctSuper));
  assertEquals([1, 0, 1, 1], wasm.testFromFuncRef(wasm.fctSub));

  assertEquals([1, 1, 1, 1], wasm.testNullFromFuncRef(null));
  assertEquals([1, 0, 0, 0], wasm.testNullFromFuncRef(jsFct));
  assertEquals([1, 0, 1, 0], wasm.testNullFromFuncRef(wasm.fctSuper));
  assertEquals([1, 0, 1, 1], wasm.testNullFromFuncRef(wasm.fctSub));
})();

(function RefCastFuncRef() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sigSuper = builder.addType(makeSig([kWasmI32], []));
  let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

  builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
  builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
  builder.addFunction('castToFuncRef', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kFuncRefCode])
    .exportFunc();
  builder.addFunction('castToNullFuncRef',
                      makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kNullFuncRefCode])
    .exportFunc();
  builder.addFunction('castToSuper', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, sigSuper])
    .exportFunc();
  builder.addFunction('castToSub', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, sigSub])
    .exportFunc();

  builder.addFunction('castNullToFuncRef',
                      makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kFuncRefCode])
    .exportFunc();
  builder.addFunction('castNullToNullFuncRef',
                      makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kNullFuncRefCode])
    .exportFunc();
  builder.addFunction('castNullToSuper',
                      makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, sigSuper])
    .exportFunc();
  builder.addFunction('castNullToSub', makeSig([kWasmFuncRef], [kWasmFuncRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, sigSub])
    .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let jsFct = new WebAssembly.Function(
      {parameters:['i32', 'i32'], results: ['i32']},
      function mul(a, b) { return a * b; });

  assertTraps(kTrapIllegalCast, () => wasm.castToFuncRef(null));
  assertSame(jsFct, wasm.castToFuncRef(jsFct));
  assertSame(wasm.fctSuper, wasm.castToFuncRef(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castToFuncRef(wasm.fctSub));

  assertSame(null, wasm.castNullToFuncRef(null));
  assertSame(jsFct, wasm.castNullToFuncRef(jsFct));
  assertSame(wasm.fctSuper, wasm.castNullToFuncRef(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castNullToFuncRef(wasm.fctSub));

  assertSame(null, wasm.castNullToNullFuncRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullFuncRef(jsFct));
  assertTraps(kTrapIllegalCast,
              () => wasm.castNullToNullFuncRef(wasm.fctSuper));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullFuncRef(wasm.fctSub));

  assertTraps(kTrapIllegalCast, () => wasm.castToSuper(null));
  assertTraps(kTrapIllegalCast, () => wasm.castToSuper(jsFct));
  assertSame(wasm.fctSuper, wasm.castToSuper(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castToSuper(wasm.fctSub));

  assertSame(null, wasm.castNullToSuper(null));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToSuper(jsFct));
  assertSame(wasm.fctSuper, wasm.castNullToSuper(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castNullToSuper(wasm.fctSub));

  assertTraps(kTrapIllegalCast, () => wasm.castToSub(null));
  assertTraps(kTrapIllegalCast, () => wasm.castToSub(jsFct));
  assertTraps(kTrapIllegalCast, () => wasm.castToSub(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castToSub(wasm.fctSub));

  assertSame(null, wasm.castNullToSub(null));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToSub(jsFct));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToSub(wasm.fctSuper));
  assertSame(wasm.fctSub, wasm.castNullToSub(wasm.fctSub));
})();

(function BrOnCastFuncRef() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sigSuper = builder.addType(makeSig([kWasmI32], []));
  let sigSub = builder.addType(makeSig([kWasmI32], []), sigSuper);

  builder.addFunction('fctSuper', sigSuper).addBody([]).exportFunc();
  builder.addFunction('fctSub', sigSub).addBody([]).exportFunc();
  let targets = {
    "funcref": kFuncRefCode,
    "nullfuncref": kNullFuncRefCode,
    "super": sigSuper,
    "sub": sigSub
  };
  for (const [name, typeCode] of Object.entries(targets)) {
    builder.addFunction(`brOnCast_${name}`,
      makeSig([kWasmFuncRef], [kWasmI32]))
    .addBody([
      kExprBlock, kWasmRef, typeCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCast, 0, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();

    builder.addFunction(`brOnCastNull_${name}`,
      makeSig([kWasmFuncRef], [kWasmI32]))
    .addBody([
      kExprBlock, kWasmRefNull, typeCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCastNull, 0, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
      kExprReturn,
    ]).exportFunc();
  }

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let jsFct = new WebAssembly.Function(
      {parameters:['i32', 'i32'], results: ['i32']},
      function mul(a, b) { return a * b; });
  assertEquals(0, wasm.brOnCast_funcref(null));
  assertEquals(0, wasm.brOnCast_nullfuncref(null));
  assertEquals(0, wasm.brOnCast_super(null));
  assertEquals(0, wasm.brOnCast_sub(null));

  assertEquals(1, wasm.brOnCast_funcref(jsFct));
  assertEquals(0, wasm.brOnCast_nullfuncref(jsFct));
  assertEquals(0, wasm.brOnCast_super(jsFct));
  assertEquals(0, wasm.brOnCast_sub(jsFct));

  assertEquals(1, wasm.brOnCast_funcref(wasm.fctSuper));
  assertEquals(0, wasm.brOnCast_nullfuncref(wasm.fctSuper));
  assertEquals(1, wasm.brOnCast_super(wasm.fctSuper));
  assertEquals(0, wasm.brOnCast_sub(wasm.fctSuper));

  assertEquals(1, wasm.brOnCast_funcref(wasm.fctSub));
  assertEquals(0, wasm.brOnCast_nullfuncref(wasm.fctSub));
  assertEquals(1, wasm.brOnCast_super(wasm.fctSub));
  assertEquals(1, wasm.brOnCast_sub(wasm.fctSub));

  assertEquals(1, wasm.brOnCastNull_funcref(null));
  assertEquals(1, wasm.brOnCastNull_nullfuncref(null));
  assertEquals(1, wasm.brOnCastNull_super(null));
  assertEquals(1, wasm.brOnCastNull_sub(null));

  assertEquals(1, wasm.brOnCastNull_funcref(jsFct));
  assertEquals(0, wasm.brOnCastNull_nullfuncref(jsFct));
  assertEquals(0, wasm.brOnCastNull_super(jsFct));
  assertEquals(0, wasm.brOnCastNull_sub(jsFct));

  assertEquals(1, wasm.brOnCastNull_funcref(wasm.fctSuper));
  assertEquals(0, wasm.brOnCastNull_nullfuncref(wasm.fctSuper));
  assertEquals(1, wasm.brOnCastNull_super(wasm.fctSuper));
  assertEquals(0, wasm.brOnCastNull_sub(wasm.fctSuper));

  assertEquals(1, wasm.brOnCastNull_funcref(wasm.fctSub));
  assertEquals(0, wasm.brOnCastNull_nullfuncref(wasm.fctSub));
  assertEquals(1, wasm.brOnCastNull_super(wasm.fctSub));
  assertEquals(1, wasm.brOnCastNull_sub(wasm.fctSub));
})();

(function RefTestExternRef() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('testExternRef',
      makeSig([kWasmExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kExternRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTest, kNullExternRefCode,
    ]).exportFunc();

  builder.addFunction('testNullExternRef',
      makeSig([kWasmExternRef], [kWasmI32, kWasmI32]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kExternRefCode,
      kExprLocalGet, 0, kGCPrefix, kExprRefTestNull, kNullExternRefCode,
    ]).exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  assertEquals([0, 0], wasm.testExternRef(null));
  assertEquals([1, 0], wasm.testExternRef(undefined));
  assertEquals([1, 0], wasm.testExternRef(1));
  assertEquals([1, 0], wasm.testExternRef({}));
  assertEquals([1, 0], wasm.testExternRef(wasm.testExternRef));

  assertEquals([1, 1], wasm.testNullExternRef(null));
  assertEquals([1, 0], wasm.testNullExternRef(undefined));
  assertEquals([1, 0], wasm.testNullExternRef(1));
  assertEquals([1, 0], wasm.testNullExternRef({}));
  assertEquals([1, 0], wasm.testNullExternRef(wasm.testExternRef));
})();

(function RefCastExternRef() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('castToExternRef',
      makeSig([kWasmExternRef], [kWasmExternRef]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kExternRefCode])
    .exportFunc();
  builder.addFunction('castToNullExternRef',
    makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCast, kNullExternRefCode])
  .exportFunc();
  builder.addFunction('castNullToExternRef',
    makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kExternRefCode])
  .exportFunc();
  builder.addFunction('castNullToNullExternRef',
    makeSig([kWasmExternRef], [kWasmExternRef]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprRefCastNull, kNullExternRefCode])
  .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;

  assertTraps(kTrapIllegalCast, () => wasm.castToExternRef(null));
  assertEquals(undefined, wasm.castToExternRef(undefined));
  assertEquals(1, wasm.castToExternRef(1));
  let obj = {};
  assertSame(obj, wasm.castToExternRef(obj));
  assertSame(wasm.castToExternRef, wasm.castToExternRef(wasm.castToExternRef));

  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(1));
  assertTraps(kTrapIllegalCast, () => wasm.castToNullExternRef(obj));
  assertTraps(kTrapIllegalCast,
              () => wasm.castToNullExternRef(wasm.castToExternRef));

  assertSame(null, wasm.castNullToExternRef(null));
  assertEquals(undefined, wasm.castNullToExternRef(undefined));
  assertEquals(1, wasm.castNullToExternRef(1));
  assertSame(obj, wasm.castNullToExternRef(obj));
  assertSame(wasm.castToExternRef,
             wasm.castNullToExternRef(wasm.castToExternRef));

  assertSame(null, wasm.castNullToNullExternRef(null));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExternRef(undefined));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExternRef(1));
  assertTraps(kTrapIllegalCast, () => wasm.castNullToNullExternRef(obj));
  assertTraps(kTrapIllegalCast,
              () => wasm.castNullToNullExternRef(wasm.castToExternRef));
})();

(function BrOnCastExternRef() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('castToExternRef',
    makeSig([kWasmExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, kExternRefCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCast, 0, kExternRefCode,
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castToNullExternRef',
    makeSig([kWasmExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, kNullExternRefCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCast, 0, kNullExternRefCode,
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  builder.addFunction('castNullToExternRef',
    makeSig([kWasmExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, kExternRefCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCastNull, 0, kExternRefCode,
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();
  builder.addFunction('castNullToNullExternRef',
    makeSig([kWasmExternRef], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRefNull, kNullExternRefCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCastNull, 0, kNullExternRefCode,
      kExprI32Const, 0,
      kExprReturn,
    kExprEnd,
    kExprDrop,
    kExprI32Const, 1,
    kExprReturn,
  ])
  .exportFunc();

  let instance = builder.instantiate();
  let wasm = instance.exports;
  let obj = {};

  assertEquals(0, wasm.castToExternRef(null));
  assertEquals(1, wasm.castToExternRef(undefined));
  assertEquals(1, wasm.castToExternRef(1));
  assertEquals(1, wasm.castToExternRef(obj));
  assertEquals(1, wasm.castToExternRef(wasm.castToExternRef));

  assertEquals(0, wasm.castToNullExternRef(null));
  assertEquals(0, wasm.castToNullExternRef(undefined));
  assertEquals(0, wasm.castToNullExternRef(1));
  assertEquals(0, wasm.castToNullExternRef(obj));
  assertEquals(0, wasm.castToNullExternRef(wasm.castToExternRef));

  assertEquals(1, wasm.castNullToExternRef(null));
  assertEquals(1, wasm.castNullToExternRef(undefined));
  assertEquals(1, wasm.castNullToExternRef(1));
  assertEquals(1, wasm.castNullToExternRef(obj));
  assertEquals(1, wasm.castNullToExternRef(wasm.castToExternRef));

  assertEquals(1, wasm.castNullToNullExternRef(null));
  assertEquals(0, wasm.castNullToNullExternRef(undefined));
  assertEquals(0, wasm.castNullToNullExternRef(1));
  assertEquals(0, wasm.castNullToNullExternRef(obj));
  assertEquals(0, wasm.castNullToNullExternRef(wasm.castToExternRef));
})();


(function RefTestAnyRefHierarchy() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let structSuper = builder.addStruct([makeField(kWasmI32, true)]);
  let structSub = builder.addStruct([makeField(kWasmI32, true)], structSuper);
  let array = builder.addArray(kWasmI32);

  let types = {
    any: kWasmAnyRef,
    eq: kWasmEqRef,
    struct: kWasmStructRef,
    anyArray: kWasmArrayRef,
    array: wasmRefNullType(array),
    structSuper: wasmRefNullType(structSuper),
    structSub: wasmRefNullType(structSub),
    nullref: kWasmNullRef,
  };

  let createBodies = {
    nullref: [kExprRefNull, kNullRefCode],
    i31ref: [kExprI32Const, 42, kGCPrefix, kExprI31New],
    structSuper: [kExprI32Const, 42, kGCPrefix, kExprStructNew, structSuper],
    structSub: [kExprI32Const, 42, kGCPrefix, kExprStructNew, structSub],
    array: [kExprI32Const, 42, kGCPrefix, kExprArrayNewFixed, array, 1],
  };

  // Each Test lists the following:
  // source  => The static type of the source value.
  // values  => All actual values that are subtypes of the static types.
  // targets => A list of types for ref.test. For each type the values are
  //            listed for which ref.test should return 1 (i.e. the ref.test
  //            should succeed).
  let tests = [
    {
      source: 'any',
      values: ['nullref', 'i31ref', 'structSuper', 'structSub', 'array'],
      targets: {
        any: ['i31ref', 'structSuper', 'structSub', 'array'],
        eq: ['i31ref', 'structSuper', 'structSub', 'array'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      }
    },
    {
      source: 'eq',
      values: ['nullref', 'i31ref', 'structSuper', 'structSub', 'array'],
      targets: {
        eq: ['i31ref', 'structSuper', 'structSub', 'array'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      }
    },
    {
      source: 'struct',
      values: ['nullref', 'structSuper', 'structSub'],
      targets: {
        eq: ['structSuper', 'structSub'],
        struct: ['structSuper', 'structSub'],
        anyArray: ['array'],
        array: ['array'],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      }
    },
    {
      source: 'anyArray',
      values: ['nullref', 'array'],
      targets: {
        eq: ['array'],
        struct: [],
        anyArray: ['array'],
        array: ['array'],
        structSuper: [],
        structSub: [],
        nullref: [],
      }
    },
    {
      source: 'structSuper',
      values: ['nullref', 'structSuper', 'structSub'],
      targets: {
        eq: ['structSuper', 'structSub'],
        struct: ['structSuper', 'structSub'],
        anyArray: [],
        array: [],
        structSuper: ['structSuper', 'structSub'],
        structSub: ['structSub'],
        nullref: [],
      }
    },
  ];

  for (let test of tests) {
    let sourceType = types[test.source];
    // Add creator functions.
    let creatorSig = makeSig([], [sourceType]);
    let creatorType = builder.addType(creatorSig);
    for (let value of test.values) {
      builder.addFunction(`create_${test.source}_${value}`, creatorType)
      .addBody(createBodies[value]).exportFunc();
    }
    // Add ref.test tester functions.
    // The functions take the creator functions as a callback to prevent the
    // compiler to derive the actual type of the value and can only use the
    // static source type.
    for (let target in test.targets) {
      // Get heap type for concrete types or apply Leb128 mask on the abstract
      // type.
      let heapType = types[target].heap_type ?? (types[target] & kLeb128Mask);
      builder.addFunction(`test_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefTest, heapType,
      ]).exportFunc();
      builder.addFunction(`test_null_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefTestNull, heapType,
      ]).exportFunc();

      builder.addFunction(`cast_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefCast, heapType,
        kExprRefIsNull, // We can't expose the cast object to JS in most cases.
      ]).exportFunc();

      builder.addFunction(`cast_null_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprCallRef, ...wasmUnsignedLeb(creatorType),
        kGCPrefix, kExprRefCastNull, heapType,
        kExprRefIsNull, // We can't expose the cast object to JS in most cases.
      ]).exportFunc();

      builder.addFunction(`brOnCast_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprBlock, kWasmRef, heapType,
          kExprLocalGet, 0,
          kExprCallRef, ...wasmUnsignedLeb(creatorType),
          kGCPrefix, kExprBrOnCast, 0, heapType,
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ])
      .exportFunc();

      builder.addFunction(`brOnCastNull_${test.source}_to_${target}`,
                          makeSig([wasmRefType(creatorType)], [kWasmI32]))
      .addBody([
        kExprBlock, kWasmRefNull, heapType,
          kExprLocalGet, 0,
          kExprCallRef, ...wasmUnsignedLeb(creatorType),
          kGCPrefix, kExprBrOnCastNull, 0, heapType,
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
        kExprDrop,
        kExprI32Const, 1,
        kExprReturn,
      ])
      .exportFunc();
    }
  }

  let instance = builder.instantiate();
  let wasm = instance.exports;

  for (let test of tests) {
    for (let [target, validValues] of Object.entries(test.targets)) {
      for (let value of test.values) {
        // print(`Test ref.test: ${test.source}(${value}) -> ${target}`);
        let create_value = wasm[`create_${test.source}_${value}`];
        let res = wasm[`test_${test.source}_to_${target}`](create_value);
        assertEquals(validValues.includes(value) ? 1 : 0, res);
        // print(`Test ref.test null: ${test.source}(${value}) -> ${target}`);
        res = wasm[`test_null_${test.source}_to_${target}`](create_value);
        assertEquals(
            (validValues.includes(value) || value == "nullref") ? 1 : 0, res);

        // print(`Test ref.cast: ${test.source}(${value}) -> ${target}`);
        let cast = wasm[`cast_${test.source}_to_${target}`];
        if (validValues.includes(value)) {
          assertEquals(0, cast(create_value));
        } else {
          assertTraps(kTrapIllegalCast, () => cast(create_value));
        }
        let castNull = wasm[`cast_null_${test.source}_to_${target}`];
        if (validValues.includes(value) || value == "nullref") {
          let expected = value == "nullref" ? 1 : 0;
          assertEquals(expected, castNull(create_value));
        } else {
          assertTraps(kTrapIllegalCast, () => castNull(create_value));
        }

        // print(`Test br_on_cast: ${test.source}(${value}) -> ${target}`);
        res = wasm[`brOnCast_${test.source}_to_${target}`](create_value);
        assertEquals(validValues.includes(value) ? 1 : 0, res);
        // print(`Test br_on_cast null: ${test.source}(${value}) -> ${target}`);
        res = wasm[`brOnCastNull_${test.source}_to_${target}`](create_value);
        assertEquals(
            validValues.includes(value) || value == "nullref" ? 1 : 0, res);
      }
    }
  }
})();
