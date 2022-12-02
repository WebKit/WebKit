//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true")
import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

async function testRefTypeLocal() {
  /*
   * (module
   *   (type (func (param i32) (result i32)))
   *   (func (result (ref null 0)) (local (ref null 0))
   *     (local.get 0)))
   */
  new WebAssembly.Instance(
    module(
      "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x60\x01\x7f\x01\x7f\x60\x00\x01\x6c\x00\x03\x02\x01\x01\x0a\x09\x01\x07\x01\x01\x6c\x00\x20\x00\x0b"
    )
  );
}

async function testNonNullRefTypeLocal() {
  /*
   * (module
   *   (type (func (param i32) (result i32)))
   *   (func (local (ref 0))))
   */
  assert.throws(
    () =>
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x09\x02\x60\x01\x7f\x01\x7f\x60\x00\x00\x03\x02\x01\x01\x0a\x07\x01\x05\x01\x01\x6b\x00\x0b"
      ),
    WebAssembly.CompileError,
    "Function locals must have a defaultable type"
  );
}

async function testRefTypeInSignature() {
  /*
   * (module
   *   (elem declare funcref (ref.func $f))
   *   (type $t1 (func (param i32) (result i32)))
   *   (type $t2 (func (param) (result (ref $t1))))
   *   (func $f (type $t1) (i32.const 1))
   *   (func $g (type $t2) (ref.func $f)))
   */
  new WebAssembly.Instance(
    module(
      "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x60\x01\x7f\x01\x7f\x60\x00\x01\x6b\x00\x03\x03\x02\x00\x01\x09\x05\x01\x03\x00\x01\x00\x0a\x0b\x02\x04\x00\x41\x01\x0b\x04\x00\xd2\x00\x0b\x00\x0e\x04\x6e\x61\x6d\x65\x01\x07\x02\x00\x01\x66\x01\x01\x67"
    )
  );
}

async function testRefTypeParamCheck() {
  const wat1 = `
    (module
      (func (export "f") (param f64) (result f64)
        (local.get 0)))
    `;

  const instance1 = await instantiate(wat1);

  /*
   * (module
   *   (type $t1 (func (param i32) (result i32)))
   *   (type $t2 (func (param (ref $t1)) (result)))
   *   (func (export "f") (type $t2)))
   */
  const m2 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x60\x01\x7f\x01\x7f\x60\x01\x6b\x00\x00\x03\x02\x01\x01\x07\x05\x01\x01\x66\x00\x00\x0a\x04\x01\x02\x00\x0b"
  );
  const instance2 = new WebAssembly.Instance(m2);

  /*
   * (module
   *   (type $t1 (func (param i32) (result i32)))
   *   (type $t2 (func (param (ref null $t1)) (result)))
   *   (func (export "f") (type $t2)))
   */
  const m3 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x60\x01\x7f\x01\x7f\x60\x01\x6c\x00\x00\x03\x02\x01\x01\x07\x05\x01\x01\x66\x00\x00\x0a\x04\x01\x02\x00\x0b"
  );
  const instance3 = new WebAssembly.Instance(m3);

  for (let i=0; i<1000; ++i) {
    // Trigger the ic path
    assert.throws(
      () => instance2.exports.f(null),
      WebAssembly.RuntimeError,
      "Funcref must be an exported wasm function"
    );
    assert.throws(
      () => instance2.exports.f(instance1.exports.f),
      WebAssembly.RuntimeError,
      "Argument function did not match the reference type"
    );
    instance3.exports.f(null);
  }
}

async function testRefGlobalCheck() {
  const wat = `
    (module
      (global (export "g") funcref (ref.null func))
      (func (export "f") (param f64) (result f64)
        (local.get 0)))
    `;

  const providerInstance = await instantiate(
    wat,
    {},
    { reference_types: true }
  );

  /*
   * (module
   *   (global (export "g") (mut (ref func)) (ref.func $f))
   *   (func $f))
   */
  const m1 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x06\x07\x01\x6b\x70\x01\xd2\x00\x0b\x07\x05\x01\x01\x67\x03\x00\x0a\x04\x01\x02\x00\x0b\x00\x0b\x04\x6e\x61\x6d\x65\x01\x04\x01\x00\x01\x66"
  );
  const instance1 = new WebAssembly.Instance(m1);
  assert.throws(
    () => (instance1.exports.g.value = null),
    WebAssembly.RuntimeError,
    "Funcref must be an exported wasm function"
  );

  /*
   * (module
   *   (type (func))
   *   (global (export "g") (mut (ref 0)) (ref.func $f))
   *   (func $f (type 0)))
   */
  const m2 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x06\x07\x01\x6b\x00\x01\xd2\x00\x0b\x07\x05\x01\x01\x67\x03\x00\x0a\x04\x01\x02\x00\x0b\x00\x0b\x04\x6e\x61\x6d\x65\x01\x04\x01\x00\x01\x66"
  );
  const instance2 = new WebAssembly.Instance(m2);
  assert.throws(
    () => (instance2.exports.g.value = null),
    WebAssembly.RuntimeError,
    "Funcref must be an exported wasm function"
  );
  assert.throws(
    () => (instance2.exports.g.value = providerInstance.exports.f),
    WebAssembly.RuntimeError,
    "Argument function did not match the reference type"
  );

  /*
   * (module
   *   (import "m" "g" (global (ref func))))
   */
  const m3 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x09\x01\x01\x6d\x01\x67\x03\x6b\x70\x00"
  );
  assert.throws(
    () =>
      new WebAssembly.Instance(m3, { m: { g: providerInstance.exports.g } }),
    WebAssembly.LinkError,
    "imported global m:g must be a same type"
  );

  /*
   * (module
   *   (type (func))
   *   (import "m" "g" (global (ref 0))))
   */
  const m4 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x02\x09\x01\x01\x6d\x01\x67\x03\x6b\x00\x00"
  );
  assert.throws(
    () =>
      new WebAssembly.Instance(m4, { m: { g: providerInstance.exports.g } }),
    WebAssembly.LinkError,
    "imported global m:g must be a same type"
  );

  /*
   * (module
   *   (import "m" "g" (global (ref extern))))
   */
  const m5 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x09\x01\x01\x6d\x01\x67\x03\x6b\x6f\x00"
  );
  assert.throws(
    () => new WebAssembly.Instance(m5, { m: { g: null } }),
    WebAssembly.LinkError,
    "imported global m:g must be a non-null value"
  );

  /*
   * (module
   *   (global $g (import "m" "g") (mut (ref extern)))
   *   (func (global.set $g (ref.null extern))))
   */
  assert.throws(
    () => {
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x02\x09\x01\x01\x6d\x01\x67\x03\x6b\x6f\x01\x03\x02\x01\x00\x0a\x08\x01\x06\x00\xd0\x6f\x24\x00\x0b"
      )
    },
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: set_global 0 with type Externref with a variable of type Externref, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
  );
}

async function testExternFuncrefNonNullCheck() {
  /*
   * (module
   *   (type $t (func (param (ref extern)) (result)))
   *   (func (export "f") (type $t)))
   */
  const m1 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x06\x01\x60\x01\x6b\x6f\x00\x03\x02\x01\x00\x07\x05\x01\x01\x66\x00\x00\x0a\x04\x01\x02\x00\x0b"
  );
  const instance1 = new WebAssembly.Instance(m1);

  /*
   * (module
   *   (type $t (func (param (ref func)) (result)))
   *   (func (export "f") (type $t)))
   */
  const m2 = module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x06\x01\x60\x01\x6b\x70\x00\x03\x02\x01\x00\x07\x05\x01\x01\x66\x00\x00\x0a\x04\x01\x02\x00\x0b"
  );
  const instance2 = new WebAssembly.Instance(m2);

  for (let i=0; i<1000; ++i) {
    // Trigger the ic path
    assert.throws(
      () => instance1.exports.f(null),
      WebAssembly.RuntimeError,
      "Non-null Externref cannot be null"
    );
    assert.throws(
      () => instance2.exports.f(null),
      WebAssembly.RuntimeError,
      "Funcref must be an exported wasm function"
    );
  }
}

// Ensure two ways of writing externref are equivalent.
async function testExternrefCompatibility() {
  /*
   * (module
   *   (type $t (func (param externref) (result (ref null extern))))
   *   (func $f (type $t) (local.get 0)))
   */
  module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x60\x01\x6f\x01\x6c\x6f\x03\x02\x01\x00\x0a\x06\x01\x04\x00\x20\x00\x0b\x00\x0b\x04\x6e\x61\x6d\x65\x01\x04\x01\x00\x01\x66"
  );
}

async function testNonNullExternrefIncompatible() {
  /*
   * (module
   *   (type $t (func (param externref) (result (ref extern))))
   *   (func $f (type $t) (local.get 0)))
   */
  assert.throws(
    () =>
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x60\x01\x6f\x01\x6b\x6f\x03\x02\x01\x00\x0a\x06\x01\x04\x00\x20\x00\x0b\x00\x0b\x04\x6e\x61\x6d\x65\x01\x04\x01\x00\x01\x66"
      ),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. Externref is not a Externref, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
  );
}

// Ensure two ways of writing funcref are equivalent.
async function testFuncrefCompatibility() {
  /*
   * (module
   *   (type $t (func (param funcref) (result (ref null func))))
   *   (func $f (type $t) (local.get 0)))
   */
  module(
    "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x60\x01\x70\x01\x6c\x70\x03\x02\x01\x00\x0a\x06\x01\x04\x00\x20\x00\x0b\x00\x0b\x04\x6e\x61\x6d\x65\x01\x04\x01\x00\x01\x66"
  );
}

async function testNonNullFuncrefIncompatible() {
  /*
   * (module
   *   (type $t (func (param funcref) (result (ref func))))
   *   (func $f (type $t) (local.get 0)))
   */
  assert.throws(
    () =>
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x60\x01\x70\x01\x6b\x70\x03\x02\x01\x00\x0a\x06\x01\x04\x00\x20\x00\x0b\x00\x0b\x04\x6e\x61\x6d\x65\x01\x04\x01\x00\x01\x66"
      ),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. Funcref is not a Funcref, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
  );
}

async function testWasmJSGlobals() {
  const providerInstance = await instantiate(
    `(module
        (func (export "f") (param f64) (result f64)
          (local.get 0))
     )`,
    {},
    { reference_types: true }
  );

  const wasmGlobalFuncref = new WebAssembly.Global({value:'funcref', mutable:true});

  // Null is OK, because Funcref in typed function references proposal represents (ref null funcref).
  assert.eq(wasmGlobalFuncref.value, null);
  wasmGlobalFuncref.value = null;
  assert.eq(wasmGlobalFuncref.value, null);

  // A wasm function from other instance is OK too because (ref $t) <: (ref null funcref)
  wasmGlobalFuncref.value = providerInstance.exports.f;

  assert.throws(
    () => wasmGlobalFuncref.value = console.log,
    WebAssembly.RuntimeError,
    "Funcref must be an exported wasm function (evaluating 'wasmGlobalFuncref.value = console.log')"
  );

  const wasmGlobalExtern = new WebAssembly.Global({value:'externref', mutable:true});
  assert.eq(wasmGlobalExtern.value, undefined);
  wasmGlobalExtern.value = console.log;
  assert.eq(wasmGlobalExtern.value, console.log);

  wasmGlobalExtern.value = providerInstance.exports.f;
  assert.eq(wasmGlobalExtern.value, providerInstance.exports.f);
}

async function testRefTypesInTables() {
  const providerInstance = await instantiate(
    `(module
        (func (export "f") (param f64) (result f64)
          (local.get 0))
     )`,
    {},
    { reference_types: true }
  );

  const wasmTableFuncref = new WebAssembly.Table({ initial: 1, maximum: 1, element: "funcref" });
  assert.eq(wasmTableFuncref.get(0), null);
  wasmTableFuncref.set(0, providerInstance.exports.f);
  assert.eq(wasmTableFuncref.get(0), providerInstance.exports.f);

  assert.throws(
    () => wasmTableFuncref.set(0, console.log),
    TypeError,
    "WebAssembly.Table.prototype.set expects the second argument to be null or an instance of WebAssembly.Function"
  );

  const wasmTableExternref = new WebAssembly.Table({ initial: 1, maximum: 1, element: "externref" });
  assert.eq(wasmTableExternref.get(0), undefined);
  wasmTableExternref.set(0, providerInstance.exports.f);
  assert.eq(wasmTableExternref.get(0), providerInstance.exports.f);

  wasmTableExternref.set(0, console.log);
  assert.eq(wasmTableExternref.get(0), console.log);
}

assert.asyncTest(testRefTypeLocal());
assert.asyncTest(testNonNullRefTypeLocal());
assert.asyncTest(testRefTypeInSignature());
assert.asyncTest(testRefTypeParamCheck());
assert.asyncTest(testRefGlobalCheck());
assert.asyncTest(testExternFuncrefNonNullCheck());
assert.asyncTest(testExternrefCompatibility());
assert.asyncTest(testNonNullExternrefIncompatible());
assert.asyncTest(testFuncrefCompatibility());
assert.asyncTest(testNonNullFuncrefIncompatible());
assert.asyncTest(testWasmJSGlobals());
assert.asyncTest(testRefTypesInTables());
