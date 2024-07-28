//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: TypeError: WebAssembly.Table.prototype.grow expects the second argument to be null or an instance of WebAssembly.Function
//  grow@[native code]
//  TableGrowWithInitializer@typed-funcref.js:35:13
//  global code@typed-funcref.js:46:3

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-typed-funcref

load("wasm-module-builder.js");

// Export an arbitrary function from a Wasm module (identity).
let foo = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction('foo', kSig_i_i)
      .addBody([kExprLocalGet, 0])
      .exportAs('foo');
  let module = new WebAssembly.Module(builder.toBuffer());
  return (new WebAssembly.Instance(builder.toModule())).exports.foo;
})();

(function TableGrowWithInitializer() {
  // print(arguments.callee.name);
  var table =
      new WebAssembly.Table({element: 'anyfunc', initial: 0, maximum: 100});

  table.grow(10);
  table.grow(10, foo);
  table.grow(10, null);
  table.grow(10, undefined);

  for (let i = 0; i < 10; i++) {
    assertNull(table.get(i));
  }
  for (let i = 10; i < 20; i++) {
    assertEquals(foo, table.get(i));
  }
  for (let i = 20; i < 40; i++) {
    assertNull(table.get(i));
  }
})();
