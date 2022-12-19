//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Failure (Error message):
//  expected:
//  should match '/Duplicate export name 'main' for function 0 and function 2/'
//  found:
//  "WebAssembly.Module doesn't parse at byte 49: duplicate export: 'main' (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')"
//
//  Stack: MjsUnitAssertionError@mjsunit.js:36:27
//  failWithMessage@mjsunit.js:323:36
//  fail@mjsunit.js:343:27
//  assertMatches@mjsunit.js:599:11
//  checkException@mjsunit.js:501:20
//  assertThrows@mjsunit.js:518:21
//  testExportNameClash@export-table.js:134:15
//  global code@export-table.js:136:3
//  MjsUnitAssertionError@mjsunit.js:36:27
//  failWithMessage@mjsunit.js:323:36
//  fail@mjsunit.js:343:27
//  assertMatches@mjsunit.js:599:11
//  checkException@mjsunit.js:501:20
//  assertThrows@mjsunit.js:518:21
//  testExportNameClash@export-table.js:134:15
//  global code@export-table.js:136:3

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("wasm-module-builder.js");

(function testExportedMain() {
  // print("TestExportedMain...");
  var kReturnValue = 44;
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprI32Const,
      kReturnValue,
      kExprReturn
    ])
    .exportFunc();

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports.main);

  assertEquals(kReturnValue, module.exports.main());
})();

(function testExportedTwice() {
  // print("TestExportedTwice...");
  var kReturnValue = 45;

  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprI32Const,
      kReturnValue,
      kExprReturn
    ])
    .exportAs("blah")
    .exportAs("foo");

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports.blah);
  assertEquals("function", typeof module.exports.foo);

  assertEquals(kReturnValue, module.exports.foo());
  assertEquals(kReturnValue, module.exports.blah());
  assertSame(module.exports.blah, module.exports.foo);
})();

(function testEmptyName() {
  // print("TestEmptyName...");
  var kReturnValue = 46;

  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprI32Const,
      kReturnValue,
      kExprReturn
    ])
    .exportAs("");

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports[""]);

  assertEquals(kReturnValue, module.exports[""]());
})();

(function testNumericName() {
  // print("TestNumericName...");
  var kReturnValue = 47;

  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprI32Const,
      kReturnValue,
      kExprReturn
    ])
    .exportAs("0");

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports["0"]);

  assertEquals(kReturnValue, module.exports["0"]());
})();

(function testExportNameClash() {
  // print("TestExportNameClash...");
  var builder = new WasmModuleBuilder();

  builder.addFunction("one",   kSig_v_v).addBody([kExprNop]).exportAs("main");
  builder.addFunction("two",   kSig_v_v).addBody([kExprNop]).exportAs("other");
  builder.addFunction("three", kSig_v_v).addBody([kExprNop]).exportAs("main");

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /Duplicate export name 'main' for function 0 and function 2/);
})();


(function testExportMultipleIdentity() {
  // print("TestExportMultipleIdentity...");
  var builder = new WasmModuleBuilder();

  var f = builder.addFunction("one", kSig_v_v).addBody([kExprNop])
    .exportAs("a")
    .exportAs("b")
    .exportAs("c");

  let instance = builder.instantiate();
  let e = instance.exports;
  assertEquals("function", typeof e.a);
  assertEquals("function", typeof e.b);
  assertEquals("function", typeof e.c);
  assertSame(e.a, e.b);
  assertSame(e.a, e.c);
  assertEquals(String(f.index), e.a.name);
})();


(function testReexportJSMultipleIdentity() {
  // print("TestReexportMultipleIdentity...");
  var builder = new WasmModuleBuilder();

  function js() {}

  var a = builder.addImport("m", "a", kSig_v_v);
  builder.addExport("f", a);
  builder.addExport("g", a);

  let instance = builder.instantiate({m: {a: js}});
  let e = instance.exports;
  assertEquals("function", typeof e.f);
  assertEquals("function", typeof e.g);
  assertFalse(e.f == js);
  assertFalse(e.g == js);
  assertTrue(e.f == e.g);
})();


(function testReexportJSMultiple() {
  // print("TestReexportMultiple...");
  var builder = new WasmModuleBuilder();

  function js() {}

  var a = builder.addImport("q", "a", kSig_v_v);
  var b = builder.addImport("q", "b", kSig_v_v);
  builder.addExport("f", a);
  builder.addExport("g", b);

  let instance = builder.instantiate({q: {a: js, b: js}});
  let e = instance.exports;
  assertEquals("function", typeof e.f);
  assertEquals("function", typeof e.g);
  assertFalse(e.f == js);
  assertFalse(e.g == js);
  assertFalse(e.f == e.g);
})();
