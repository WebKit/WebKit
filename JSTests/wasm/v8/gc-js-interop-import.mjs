//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: TypeError: Module specifier, 'gc-js-interop-export.mjs' is not absolute and does not start with "./" or "../".
// Referenced from: .tests/wasm.yaml/wasm/v8/gc-js-interop-import.mjs

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --allow-natives-syntax

import {struct, array} from 'gc-js-interop-export.mjs';

// Read struct and array with new wasm module.
let builder = new WasmModuleBuilder();
let struct_type = builder.addStruct([makeField(kWasmI32, true)]);
let array_type = builder.addArray(kWasmI32, true);
builder.addFunction('readStruct', makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,                           // --
      kGCPrefix, kExprExternInternalize,          // --
      kGCPrefix, kExprRefAsStruct,                // --
      kGCPrefix, kExprRefCast, struct_type,       // --
      kGCPrefix, kExprStructGet, struct_type, 0,  // --
    ]);
builder.addFunction('readArrayLength', makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,                           // --
      kGCPrefix, kExprExternInternalize,          // --
      kGCPrefix, kExprRefAsArray,                 // --
      kGCPrefix, kExprArrayLen,
    ]);

let instance = builder.instantiate();
let wasm = instance.exports;
assertEquals(42, wasm.readStruct(struct));
assertEquals(2, wasm.readArrayLength(array));
assertTraps(kTrapIllegalCast, () => wasm.readStruct(array));
assertTraps(kTrapIllegalCast, () => wasm.readArrayLength(struct));
