//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
//@ $skipModes << "wasm-no-jit".to_sym
//@ $skipModes << "wasm-no-wasm-jit".to_sym
// FIXME: SIMD requires the JIT because IPInt does not interpret it; unskip once IPInt supports SIMD.
// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbo-force-mid-tier-regalloc

load('wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_i_v).addBody([
  kExprI32Const, 0,                           // i32.const
  kSimdPrefix, kExprI8x16Splat,               // i8x16.splat
  kExprI32Const, 0,                           // i32.const
  kSimdPrefix, kExprI8x16Splat,               // i8x16.splat
  kSimdPrefix, kExprI64x2Mul, 0x01,           // i64x2.mul
  kSimdPrefix, kExprI8x16ExtractLaneS, 0x00,  // i8x16.extract_lane_s
]);
builder.toModule();
