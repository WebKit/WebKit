//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: CompileError: WebAssembly.Module doesn't parse at byte 12: 0th type failed to parse because struct types are not enabled (evaluating 'new WebAssembly.Module(this.toBuffer(debug))')
//  Module@[native code] 
//  toModule@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2082:34
//  instantiate@.tests/wasm.yaml/wasm/v8/wasm-module-builder.js:2071:31
//  global code@wasm-gc-js-ref.js:31:3

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc
"use strict";

load("wasm-module-builder.js");

let instance = (() => {
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  builder.addFunction('createStruct', makeSig([kWasmI32], [kWasmEqRef]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, struct])
    .exportFunc();
  builder.addFunction('passObj', makeSig([kWasmExternRef], [kWasmExternRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();
  return builder.instantiate({});
})();

let obj = instance.exports.createStruct(123);
// The struct is opaque and doesn't have any observable properties.
assertFalse(obj instanceof Object);
assertEquals([], Object.getOwnPropertyNames(obj));
// It can be passed as externref without any observable change.
let passObj = instance.exports.passObj;
let obj2 = passObj(obj);
assertFalse(obj2 instanceof Object);
assertEquals([], Object.getOwnPropertyNames(obj2));
assertSame(obj, obj2);
// A JavaScript object can be passed as externref.
let jsObject = {"hello": "world"};
assertSame(jsObject, passObj(jsObject));
