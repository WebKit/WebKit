//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Skipping this test due to the following issues:
// call to %DisallowWasmCodegen()
// call to %IsAsmWasmCode()

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noexpose-wasm --validate-asm --allow-natives-syntax

// NOTE: This is in its own file because it calls %DisallowWasmCodegen, which
// messes with the isolate's state.
(function testAsmWithWasmOff() {
  %DisallowWasmCodegen(true);
  function Module() {
    'use asm';
    function foo() {
      return 0;
    }
    return {foo: foo};
  }
  Module();
  assertTrue(%IsAsmWasmCode(Module));
})();
