//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Skipping this test due to the following issues:
// call to %DeserializeWasmModule()
// call to %SerializeWasmModule()

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Force TurboFan code for serialization.
// Flags: --allow-natives-syntax --print-wasm-code --no-liftoff
// Flags: --no-wasm-lazy-compilation

// Just test that printing the code of the following wasm modules does not
// crash.

load("wasm-module-builder.js");

(function print_deserialized_code() {
  // https://crbug.com/849656
  // print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  builder.addImport('', 'imp', kSig_i_v);

  builder.addFunction('main', kSig_i_v)
      .addBody([
        kExprCallFunction,
        0,
      ])
      .exportFunc();

  var wire_bytes = builder.toBuffer();
  var module = new WebAssembly.Module(wire_bytes);
  // print('serializing');
  var buff = %SerializeWasmModule(module);
  // print('deserializing');
  module = %DeserializeWasmModule(buff, wire_bytes);
})();
