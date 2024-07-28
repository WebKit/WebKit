//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't validate: I32Mul right value type mismatch, in function at index 0 (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code]
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  testInstantiateLazyValidation@compilation-hints-lazy-validation.js:41:37
//  3lobal code@compilation-hints-lazy-validation.js:48:3

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --wasm-lazy-validation

load("wasm-module-builder.js");

(function testInstantiateLazyValidation() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprI64Const, 1,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierBaseline,
                             kCompilationHintTierBaseline)
         .exportFunc();

  let expected_error_msg = "Compiling function #0:\"id\" failed: i32.mul[1] " +
                           "expected type i32, found i64.const of type i64 " +
                           "@+56";
  let assertCompileErrorOnInvocation = function(instance) {
    assertThrows(() => instance.exports.id(3),
                 WebAssembly.CompileError,
                 expected_error_msg)
  };

  // Synchronous case.
  let instance = builder.instantiate();
  assertCompileErrorOnInvocation(instance);

  // Asynchronous case.
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiate(bytes)
    .then(p => assertCompileErrorOnInvocation(p.instance)));
})();
