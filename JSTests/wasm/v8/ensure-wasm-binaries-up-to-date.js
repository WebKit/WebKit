//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: ReferenceError: Can't find variable: readbuffer
//  ensure_incrementer@ensure-wasm-binaries-up-to-date.js:38:24
//  global code@ensure-wasm-binaries-up-to-date.js:43:2
// Looks like we need readbuffer().

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

// Ensure checked in wasm binaries used by integration tests from v8 hosts
// (such as chromium) are up to date.

(function print_incrementer() {
  if (true) return; // remove to regenerate the module

  load("wasm-module-builder.js");

  var module = new WasmModuleBuilder();
  module.addFunction(undefined, kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
    .exportAs("increment");

  var buffer = module.toBuffer(true);
  var view = new Uint8Array(buffer);

  // print("const unsigned char module[] = {");
  for (var i = 0; i < buffer.byteLength; i++) {
    // print("  " + view[i] + ",");
  }
  // print("};");
})();

(function ensure_incrementer() {
  var buff = readbuffer("incrementer.wasm");
  var mod = new WebAssembly.Module(buff);
  var inst = new WebAssembly.Instance(mod);
  var inc = inst.exports.increment;
  assertEquals(3, inc(2));
}())
