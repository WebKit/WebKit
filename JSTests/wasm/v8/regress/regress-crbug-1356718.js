//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.addFunction('crash', makeSig([
    kWasmF64, kWasmS128, kWasmS128, kWasmS128, kWasmS128, kWasmS128, kWasmS128,
    kWasmF64, kWasmI32
  ], [])).addBody([kExprNop]);

builder.instantiate();
