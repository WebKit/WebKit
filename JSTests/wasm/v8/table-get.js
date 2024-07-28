//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Failure: expected <Object()> found <undefined>
//
//  Stack: MjsUnitAssertionError@mjsunit.js:36:27
//  failWithMessage@mjsunit.js:323:36
//  fail@mjsunit.js:343:27
//  assertSame@mjsunit.js:403:9
//  testTableGetUniqueWrapperExportedFunction@table-get.js:76:13
//  global code@table-get.js:79:3
//  MjsUnitAssertionError@mjsunit.js:36:27
//  failWithMessage@mjsunit.js:323:36
//  fail@mjsunit.js:343:27
//  assertSame@mjsunit.js:403:9
//  testTableGetUniqueWrapperExportedFunction@table-get.js:93:13
//  global code@table-get.js:96:3

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("wasm-module-builder.js");

(function testTableGetNonExportedFunction() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 20).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f2.index]);

  const instance = builder.instantiate();

  const table_function2 = instance.exports.table.get(offset + 1);
  assertEquals(22, table_function2());
})();

(function testTableGetExportedFunction() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 20).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22])
                    .exportAs("f2");
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f2.index]);

  const instance = builder.instantiate();

  const table_function2 = instance.exports.table.get(offset + 1);
  assertEquals(22, table_function2());
})();

(function testTableGetOverlappingSegments() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 20).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f2.index]);
  builder.addActiveElementSegment(0, wasmI32Const(offset + 1),
                                  [f1.index, f2.index]);

  const instance = builder.instantiate();

  const table_function1 = instance.exports.table.get(offset + 1);
  assertEquals(11, table_function1());
})();

(function testTableGetUniqueWrapperExportedFunction() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 20).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v)
                    .addBody([kExprI32Const, 11]).exportAs("f1");
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f1.index, f1.index]);

  const instance = builder.instantiate();

  assertEquals(undefined, instance.exports.f1.tag);
  const my_tag = { hello: 15 };
  instance.exports.f1.tag = my_tag;

  assertSame(my_tag, instance.exports.table.get(offset).tag);
  assertSame(my_tag, instance.exports.table.get(offset + 1).tag);
  assertSame(my_tag, instance.exports.table.get(offset + 2).tag);
})();

(function testTableGetUniqueWrapperNonExportedFunction() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 20).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f1.index, f1.index]);

  const instance = builder.instantiate();

  assertEquals(undefined, instance.exports.table.get(offset).tag);
  const my_tag = { hello: 15 };
  instance.exports.table.get(offset).tag = my_tag;

  assertSame(my_tag, instance.exports.table.get(offset + 1).tag);
  assertSame(my_tag, instance.exports.table.get(offset + 2).tag);
})();

(function testTableGetEmptyValue() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const table = builder.addTable(kWasmAnyFunc, 20).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f1.index, f1.index]);

  const instance = builder.instantiate();
  assertEquals(null, instance.exports.table.get(offset - 1));
})();

(function testTableGetOOB() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const size = 20;
  const table = builder.addTable(kWasmAnyFunc, size).exportAs("table");
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const f2 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 22]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, f1.index, f1.index]);

  const instance = builder.instantiate();
  assertThrows(() => instance.exports.table.get(size + 3), RangeError);
})();

(function testTableGetImportedFunction() {
  // print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const size = 20;
  const table = builder.addTable(kWasmAnyFunc, size).exportAs("table");
  const import1 = builder.addImport("q", "fun", kSig_i_ii);
  const f1 = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 11]);
  const offset = 3;
  builder.addActiveElementSegment(0, wasmI32Const(offset),
                                  [f1.index, import1]);

  const instance = builder.instantiate({q: {fun: () => 33}});
  assertEquals(33, instance.exports.table.get(offset + 1)());
})();
