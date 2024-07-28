//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Failure:
//  expected:
//  "unreachable"
//  found:
//  "Unreachable code should not be executed (evaluating 'main()')"
// Looks like we need to update the exception strings.

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("wasm-module-builder.js");

var main = (function () {
  var builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_v_v)
    .addBody([kExprUnreachable])
    .exportAs("main");

  return builder.instantiate().exports.main;
})();

var exception = "";
try {
    assertEquals(0, main());
} catch(e) {
    // print("correctly caught: " + e);
    exception = e;
}
assertEquals("unreachable", exception.message);
