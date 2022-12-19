//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Did not throw exception, expected RangeError
//
//  Stack: MjsUnitAssertionError@mjsunit.js:36:27
//  failWithMessage@mjsunit.js:323:36
//  assertThrows@mjsunit.js:524:20
//  TestSyncBigModule@max-module-size-flag.js:52:15
//  global code@max-module-size-flag.js:55:3

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-module-size=128

load("wasm-module-builder.js");

let small_binary = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction('f', kSig_v_v).addBody(new Array(32).fill(kExprNop));
  return builder.toBuffer();
})();

let big_binary = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction('f', kSig_v_v).addBody(new Array(128).fill(kExprNop));
  return builder.toBuffer();
})();

// Check that the sizes of the generated modules are within the expected ranges.
assertTrue(small_binary.length > 64);
assertTrue(small_binary.length < 128);
assertTrue(big_binary.length > 128);
assertTrue(big_binary.length < 256);

let big_error_msg =
    'buffer source exceeds maximum size of 128 (is ' + big_binary.length + ')';

(function TestSyncSmallModule() {
  let sync_small_module = new WebAssembly.Module(small_binary);
  assertTrue(sync_small_module instanceof WebAssembly.Module);
})();

assertPromiseResult((async function TestAsyncSmallModule() {
  let async_small_module = await WebAssembly.compile(small_binary);
  assertTrue(async_small_module instanceof WebAssembly.Module);
})());

(function TestSyncBigModule() {
  assertThrows(
      () => new WebAssembly.Module(big_binary), RangeError,
      'WebAssembly.Module(): ' + big_error_msg);
})();

(function TestAsyncBigModule() {
  assertThrowsAsync(
      WebAssembly.compile(big_binary), RangeError,
      'WebAssembly.compile(): ' + big_error_msg);
})();
