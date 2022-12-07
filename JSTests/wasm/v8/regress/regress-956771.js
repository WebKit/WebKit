// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

load('wasm-module-builder.js');

(function testLazyModuleAsyncCompilation() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("some", kSig_i_ii)
  assertPromiseResult(
      WebAssembly.compile(builder.toBuffer())
          .then(
              assertUnreachable,
              error => assertEquals(
                  `WebAssembly.Module doesn't parse at byte 1: can't decode opcode, in function at index 0`,
                  error.message)));
})();

(function testLazyModuleSyncCompilation() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("some", kSig_i_ii)
  assertThrows(
      () => builder.toModule(), WebAssembly.CompileError,
      `WebAssembly.Module doesn't parse at byte 1: can't decode opcode, in function at index 0 (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')`);
})();
