// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh

load("wasm-module-builder.js");

// Helper function to return a new exported exception with the {kSig_v_v} type
// signature from an anonymous module. The underlying module is thrown away.
// This allows tests to reason solely about importing exceptions.
function NewExportedTag() {
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_v);
  builder.addExportOfKind("t", kExternalTag, tag);
  let instance = builder.instantiate();
  return instance.exports.t;
}

(function TestImportSimple() {
  let exported = NewExportedTag();
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);

  assertDoesNotThrow(() => builder.instantiate({ m: { ex: exported }}));
})();

(function TestImportMultiple() {
  let exported = NewExportedTag();
  let builder = new WasmModuleBuilder();
  let except1 = builder.addImportedTag("m", "ex1", kSig_v_v);
  let except2 = builder.addImportedTag("m", "ex2", kSig_v_v);
  let except3 = builder.addTag(kSig_v_v);
  builder.addExportOfKind("ex2", kExternalTag, except2);
  builder.addExportOfKind("ex3", kExternalTag, except3);
  let instance = builder.instantiate({ m: { ex1: exported, ex2: exported }});

  assertTrue(except1 < except3 && except2 < except3);
  assertEquals(undefined, instance.exports.ex1);
  // FIXME: fix broken v8 wasm exceptions tests
  // assertSame(exported, instance.exports.ex2);
  assertNotSame(exported, instance.exports.ex3);
})();

(function TestImportMissing() {
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);

  assertThrows(
      () => builder.instantiate({}), TypeError,
      /import m:ex must be an object/);
  assertThrows(
      () => builder.instantiate({ m: {}}), WebAssembly.LinkError,
      /Tag import m:ex is not an instance of WebAssembly.Tag/);
})();

(function TestImportValueMismatch() {
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);

  assertThrows(
      () => builder.instantiate({ m: { ex: 23 }}), WebAssembly.LinkError,
      /Tag import m:ex is not an instance of WebAssembly.Tag/);
  assertThrows(
      () => builder.instantiate({ m: { ex: {} }}), WebAssembly.LinkError,
      /Tag import m:ex is not an instance of WebAssembly.Tag/);
  var monkey = Object.create(NewExportedTag());
  assertThrows(
      () => builder.instantiate({ m: { ex: monkey }}), WebAssembly.LinkError,
      /Tag import m:ex is not an instance of WebAssembly.Tag/);
})();

(function TestImportSignatureMismatch() {
  let exported = NewExportedTag();
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_i);

  assertThrows(
      () => builder.instantiate({ m: { ex: exported }}), WebAssembly.LinkError,
      /imported Tag m:ex signature doesn't match the imported WebAssembly Tag's signature/);
})();

(function TestImportModuleGetImports() {
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);
  let module = new WebAssembly.Module(builder.toBuffer());

  let imports = WebAssembly.Module.imports(module);
  assertArrayEquals([{ module: "m", name: "ex", kind: "tag" }], imports);
})();
