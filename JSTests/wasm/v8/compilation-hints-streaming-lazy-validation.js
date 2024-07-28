//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: TypeError: WebAssembly.instantiateStreaming is not a function. (In 'WebAssembly.instantiateStreaming(Promise.resolve(bytes))', 'WebAssembly.instantiateStreaming' is undefined)
// testInstantiateStreamingLazyValidation@compilation-hints-streaming-lazy-validation.js:38:55
// global code@compilation-hints-streaming-lazy-validation.js:40:3

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --wasm-test-streaming --wasm-lazy-validation

load("wasm-module-builder.js");

(function testInstantiateStreamingLazyValidation() {
  // print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprI64Const, 1,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();

  let expected_error_msg = "Compiling function #0:\"id\" failed: i32.mul[1] " +
                           "expected type i32, found i64.const of type i64 " +
                           "@+56";
  let assertCompileErrorOnInvocation = function(instance) {
    assertThrows(() => instance.exports.id(3),
                 WebAssembly.CompileError,
                 expected_error_msg)
  };

  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes))
    .then(({module, instance}) => assertCompileErrorOnInvocation(instance)));
})();
