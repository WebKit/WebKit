//@ requireOptions("--useWebAssemblySIMD=1", "--useBBQJIT=1", "--webAssemblyBBQAirModeThreshold=0", "--wasmBBQUsesAir=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip if $architecture != "arm64"
// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --experimental-wasm-simd --allow-natives-syntax

load("wasm-module-builder.js");
load("exceptions-utils.js");

(function TestThrowS128Default() {
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  var kSig_v_s = makeSig([kWasmS128], []);
  var except = builder.addTag(kSig_v_s);
  builder.addFunction("throw_simd", kSig_v_v)
      .addLocals(kWasmS128, 1)
      .addBody([
        kExprLocalGet, 0,
        kExprThrow, 0,
      ])
      .exportFunc();
  var instance = builder.instantiate();

  // NOTE: changed from original test since this part of the spec is still in flux.
  assertThrows(() => instance.exports.throw_simd());
})();

(function TestThrowCatchS128Default() {
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  var kSig_v_s = makeSig([kWasmS128], []);
  var except = builder.addTag(kSig_v_s);
  builder.addFunction("throw_catch_simd", kSig_i_v)
      .addLocals(kWasmS128, 1)
      .addBody([
        kExprTry, kWasmS128,
          kExprLocalGet, 0,
          kExprThrow, 0,
        kExprCatch, except,
        kExprEnd,
        kExprLocalGet, 0,
        kSimdPrefix, kExprI32x4Eq,
        kSimdPrefix, kExprI8x16AllTrue,
      ])
      .exportFunc();
  var instance = builder.instantiate();

  assertThrows(() => instance.exports.throw_catch_simd());
})();

(function TestThrowCatchS128WithValue() {
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  var kSig_v_s = makeSig([kWasmS128], []);
  var except = builder.addTag(kSig_v_s);
  const in_idx = 0x10;   // Input index in memory.
  const out_idx = 0x20;  // Output index in memory.
  builder.addImportedMemory("env", "memory");
  builder.addFunction("throw_catch_simd", kSig_v_v)
      .addBody([
        kExprI32Const, out_idx,
        kExprTry, kWasmS128,
          kExprI32Const, in_idx,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          kExprThrow, 0,
        kExprCatch, except,
        kExprEnd,
        kSimdPrefix, kExprS128StoreMem, 0, 0,
      ])
      .exportFunc();
  var memory = new WebAssembly.Memory({initial: 1});
  var instance = builder.instantiate({env: {memory:memory}});

  var ref = [0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
             0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0];
  var array = new Uint8Array(memory.buffer);
  array.set(ref, in_idx);  // Store reference value in memory.
  assertThrows(() => instance.exports.throw_catch_simd());
  // assertArrayEquals(ref, array.slice(out_idx, out_idx + 0x10));
})();
