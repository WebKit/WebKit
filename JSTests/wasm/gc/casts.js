//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testValidation() {
  assert.throws(
    () => compile(`
      (module
        (func (export "f") (result (ref any))
          (ref.cast (ref 256) (ref.null none))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 7: can't get heap type for ref.cast, in function at index 0"
  );

  compile(`
    (module
      (func (export "f") (result (ref any))
        (ref.cast (ref any) (ref.null none))))
  `);
}

function testBasicCasts() {
  instantiate(`
     (module
       (func (export "f") (result (ref null extern))
         (ref.cast externref (ref.null extern))))
  `).exports.f();

  assert.eq(
    instantiate(`
       (module
         (func (export "f") (result i32)
           (ref.test externref (ref.null extern))))
    `).exports.f(),
    1
  );

  instantiate(`
     (module
       (func (export "f") (result (ref null func))
         (ref.cast funcref (ref.null func))))
  `).exports.f();

  assert.eq(
    instantiate(`
       (module
         (func (export "f") (result i32)
           (ref.test funcref (ref.null func))))
    `).exports.f(),
    1
  );

  instantiate(`
     (module
       (type (array i32))
       (start 0)
       (func (export "f")
         (ref.cast (ref null 0) (ref.null 0))
         drop))
  `).exports.f();

  assert.eq(
    instantiate(`
       (module
         (type (array i32))
         (func (export "f") (result i32)
           (ref.test (ref null 0) (ref.null 0))))
    `).exports.f(),
    1
  );

  assert.throws(
    () => instantiate(`
      (module
        (func (export "f") (result (ref null extern))
          (ref.cast (ref extern) (ref.null extern))))
    `).exports.f(),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.test (ref extern) (ref.null extern))))
    `).exports.f(),
    0
  );
}

function testI31Casts() {
  instantiate(`
    (module
      (start 1)
      (func (result i31ref)
        (ref.cast (ref i31) (ref.i31 (i32.const 42))))
      (func (call 0) drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.test (ref i31) (ref.i31 (i32.const 42)))))
    `).exports.f(),
    1
  )

  assert.throws(
    () => instantiate(`
      (module
        (type (array i32))
        (start 0)
        (func
          (ref.cast (ref i31) (array.new 0 (i32.const 42) (i32.const 5)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (ref.test (ref i31) (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    0
  );
}

function testFunctionCasts() {
  instantiate(`
    (module
      (type (func (param) (result i32)))
      (elem declare funcref (ref.func 0))
      (start 2)
      (func (type 0) (i32.const 42))
      (func (result funcref)
        (ref.cast (ref func) (ref.func 0)))
      (func (call 1) drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (func (param) (result i32)))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (export "f") (result i32)
          (ref.test (ref func) (ref.func 0))))
    `).exports.f(),
    1
  )

  instantiate(`
    (module
      (type (func (param) (result i32)))
      (elem declare funcref (ref.func 0))
      (start 2)
      (func (type 0) (i32.const 42))
      (func (param funcref)
        (ref.cast (ref 0) (local.get 0))
        drop)
      (func (call 1 (ref.func 0))))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (func (param) (result i32)))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (param funcref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 1 (ref.func 0))))
    `).exports.f(),
    1
  )

  // Casts should work after properties are added to exported functions.
  {
    let f = instantiate(`
      (module
        (type (func (param) (result i32)))
        (elem declare funcref (ref.func 0))
        (func (export "f") (type 0) (i32.const 42)))
    `).exports.f;

    f.x = 3;
    Object.seal(f); // Sealing transition shouldn't disrupt cast.

    instantiate(`
      (module
        (type (func (param) (result i32)))
        (func (export "g") (param funcref) (result i32)
          (call_ref 0 (ref.cast (ref 0) (local.get 0)))))
    `).exports.g(f);

    assert.eq(
      instantiate(`
        (module
          (type (func (param) (result i32)))
          (func (export "g") (param funcref) (result i32)
            (ref.test (ref 0) (local.get 0))))
      `).exports.g(f),
      1
    )
  }

  assert.throws(
    () => instantiate(`
      (module
        (start 0)
        (func
          (ref.cast (ref func) (ref.i31 (i32.const 42)))
          drop))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.cast to type (ref i31) expected a funcref"
  );

  assert.throws(
    () => instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.test (ref func) (ref.i31 (i32.const 42)))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.test to type (ref i31) expected a funcref"
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (func (param) (result i32)))
        (type (func (param) (result f32)))
        (elem declare funcref (ref.func 0))
        (start 2)
        (func (type 1) (f32.const 42))
        (func (param funcref)
          (ref.cast (ref 0) (local.get 0))
          drop)
        (func (call 1 (ref.func 0))))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (func (param) (result i32)))
        (type (func (param) (result f32)))
        (elem declare funcref (ref.func 0))
        (func (type 1) (f32.const 42))
        (func (param funcref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 1 (ref.func 0))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (func (param) (result i32)))
        (type (func (param) (result f32)))
        (elem declare funcref (ref.func 0))
        (start 1)
        (func (type 0) (i32.const 42))
        (func
          (ref.cast (ref 1) (ref.func 0))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (func (param) (result i32)))
        (type (func (param) (result f32)))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (export "f") (result i32)
          (ref.test (ref 1) (ref.func 0))))
    `).exports.f(),
    0
  );
}

function testArrayCasts() {
  instantiate(`
    (module
      (type (array i32))
      (start 1)
      (func (result arrayref)
        (ref.cast (ref array) (array.new 0 (i32.const 42) (i32.const 5))))
      (func (call 0) drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (ref.test (ref array) (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    1
  )

  instantiate(`
    (module
      (type (array i32))
      (start 1)
      (func (param arrayref)
        (ref.cast (ref 0) (local.get 0))
        drop)
      (func (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (func (param arrayref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    1
  )

  assert.throws(
    () => instantiate(`
      (module
        (start 0)
        (func
          (ref.cast (ref array) (ref.i31 (i32.const 42)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.test (ref array) (ref.i31 (i32.const 42)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (array i32))
        (type (array f32))
        (start 1)
        (func (param arrayref)
          (ref.cast (ref 0) (local.get 0))
          drop)
        (func (call 0 (array.new 1 (f32.const 42.2) (i32.const 5)))))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (type (array f32))
        (func (param arrayref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 1 (f32.const 42.2) (i32.const 5)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (array i32))
        (type (array f32))
        (start 0)
        (func
          (ref.cast (ref 1) (array.new 0 (i32.const 42) (i32.const 5)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (type (array f32))
        (func (export "f") (result i32)
          (ref.test (ref 1) (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    0
  );
}

function testStructCasts() {
  instantiate(`
    (module
      (type (struct (field i32)))
      (start 1)
      (func (result structref)
        (ref.cast (ref struct) (struct.new 0 (i32.const 42))))
      (func (call 0) drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (func (export "f") (result i32)
          (ref.test (ref struct) (struct.new 0 (i32.const 42)))))
    `).exports.f(),
    1
  )

  instantiate(`
    (module
      (type (struct (field i32)))
      (start 1)
      (func (param structref)
        (ref.cast (ref 0) (local.get 0))
        drop)
      (func (call 0 (struct.new 0 (i32.const 42)))))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (func (param structref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (struct.new 0 (i32.const 42)))))
    `).exports.f(),
    1
  )

  assert.throws(
    () => instantiate(`
      (module
        (start 0)
        (func
          (ref.cast (ref struct) (ref.i31 (i32.const 42)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.test (ref struct) (ref.i31 (i32.const 42)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (struct (field i32)))
        (type (struct (field f32)))
        (start 1)
        (func (param structref)
          (ref.cast (ref 0) (local.get 0))
          drop)
        (func (call 0 (struct.new 1 (f32.const 42.2)))))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (type (struct (field f32)))
        (func (param structref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (struct.new 1 (f32.const 42.2)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (struct (field i32)))
        (type (struct (field f32)))
        (start 0)
        (func
          (ref.cast (ref 1) (struct.new 0 (i32.const 42)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (type (struct (field f32)))
        (func (export "f") (result i32)
          (ref.test (ref 1) (struct.new 0 (i32.const 42)))))
    `).exports.f(),
    0
  );
}

function testSubtypeCasts() {
  instantiate(`
    (module
      (type (sub (array i32)))
      (type (sub 0 (array i32)))
      (start 1)
      (func (param arrayref)
        (ref.cast (ref 0) (local.get 0))
        drop)
      (func (call 0 (array.new 1 (i32.const 42) (i32.const 5)))))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (sub (array i32)))
        (type (sub 0 (array i32)))
        (func (param arrayref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 1 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    1
  );

  instantiate(`
    (module
      (type (sub (struct (field i32))))
      (type (sub 0 (struct (field i32) (field i64))))
      (start 1)
      (func (param structref)
        (ref.cast (ref 0) (local.get 0))
        drop)
      (func (call 0 (struct.new 1 (i32.const 42) (i64.const 43)))))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct (field i32) (field i64))))
        (func (param structref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (struct.new 1 (i32.const 42) (i64.const 43)))))
    `).exports.f(),
    1
  );

  instantiate(`
    (module
      (type (sub (func (result i32))))
      (type (sub 0 (func (result i32))))
      (elem declare funcref (ref.func 0))
      (start 2)
      (func (type 1) (i32.const 42))
      (func (param funcref)
        (ref.cast (ref 0) (local.get 0))
        drop)
      (func (call 1 (ref.func 0))))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (sub (func (result i32))))
        (type (sub 0 (func (result i32))))
        (elem declare funcref (ref.func 0))
        (func (type 1) (i32.const 42))
        (func (param funcref) (result i32)
          (ref.test (ref 0) (local.get 0)))
        (func (export "f") (result i32)
          (call 1 (ref.func 0))))
    `).exports.f(),
    1
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (sub (array i32)))
        (type (sub 0 (array i32)))
        (start 1)
        (func (param arrayref)
          (ref.cast (ref 1) (local.get 0))
          drop)
        (func (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (sub (array i32)))
        (type (sub 0 (array i32)))
        (func (param arrayref) (result i32)
          (ref.test (ref 1) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (sub (func (result i32))))
        (type (sub 0 (func (result i32))))
        (elem declare funcref (ref.func 0))
        (start 2)
        (func (type 0) (i32.const 42))
        (func (param funcref)
          (ref.cast (ref 1) (local.get 0))
          drop)
        (func (call 1 (ref.func 0))))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (sub (func (result i32))))
        (type (sub 0 (func (result i32))))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (param funcref) (result i32)
          (ref.test (ref 1) (local.get 0)))
        (func (export "f") (result i32)
          (call 1 (ref.func 0))))
    `).exports.f(),
    0
  );
}

function testEqCasts() {
  instantiate(`
    (module
      (type (array i32))
      (start 1)
      (func (param arrayref) (result eqref)
        (ref.cast (ref eq) (local.get 0)))
      (func
        (call 0 (array.new 0 (i32.const 42) (i32.const 5)))
        drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (func (param arrayref) (result i32)
          (ref.test (ref eq) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    1
  );

  instantiate(`
    (module
      (type (struct (field i32)))
      (start 1)
      (func (param structref) (result eqref)
        (ref.cast (ref eq) (local.get 0)))
      (func
        (call 0 (struct.new 0 (i32.const 42)))
        drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (func (param structref) (result i32)
          (ref.test (ref eq) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (struct.new 0 (i32.const 42)))))
    `).exports.f(),
    1
  );

  instantiate(`
    (module
      (start 1)
      (func (param i31ref) (result eqref)
        (ref.cast (ref eq) (local.get 0)))
      (func
        (call 0 (ref.i31 (i32.const 42)))
        drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (func (param i31ref) (result i32)
          (ref.test (ref eq) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (ref.i31 (i32.const 42)))))
    `).exports.f(),
    1
  );

  assert.throws(
    () => compile(`
      (module
        (type (func (result i32)))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (param funcref)
          (ref.cast (ref eq) (local.get 0))
          drop))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.cast to type (ref null func) expected a subtype of anyref, in function at index 1 (evaluating 'new WebAssembly.Module(binary)')"
  );
}

function testAnyCasts() {
  instantiate(`
    (module
      (type (array i32))
      (start 1)
      (func (param arrayref) (result anyref)
        (ref.cast (ref any) (local.get 0)))
      (func
        (call 0 (array.new 0 (i32.const 42) (i32.const 5)))
        drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (func (param arrayref) (result i32)
          (ref.test (ref any) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    1
  );

  instantiate(`
    (module
      (type (struct (field i32)))
      (start 1)
      (func (param structref) (result anyref)
        (ref.cast (ref any) (local.get 0)))
      (func
        (call 0 (struct.new 0 (i32.const 42)))
        drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (func (param structref) (result i32)
          (ref.test (ref any) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (struct.new 0 (i32.const 42)))))
    `).exports.f(),
    1
  );

  instantiate(`
    (module
      (start 1)
      (func (param i31ref) (result anyref)
        (ref.cast (ref any) (local.get 0)))
      (func
        (call 0 (ref.i31 (i32.const 42)))
        drop))
  `);

  assert.eq(
    instantiate(`
      (module
        (func (param i31ref) (result i32)
          (ref.test (ref any) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (ref.i31 (i32.const 42)))))
    `).exports.f(),
    1
  );

  assert.throws(
    () => compile(`
      (module
        (type (func (result i32)))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (param funcref)
          (ref.cast (ref any) (local.get 0))
          drop))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.cast to type (ref null func) expected a subtype of anyref, in function at index 1 (evaluating 'new WebAssembly.Module(binary)')"
  );
}

function testNullCasts() {
  assert.throws(
    () => instantiate(`
      (module
        (type (array i32))
        (start 1)
        (func (param arrayref) (result nullref)
          (ref.cast (ref none) (local.get 0)))
        (func
          (call 0 (array.new 0 (i32.const 42) (i32.const 5)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  )

  assert.eq(
    instantiate(`
      (module
        (type (array i32))
        (func (param arrayref) (result i32)
          (ref.test (ref none) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (array.new 0 (i32.const 42) (i32.const 5)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (start 1)
        (func (param funcref) (result nullfuncref)
          (ref.cast (ref nofunc) (local.get 0)))
        (func
          (call 0 (ref.null func))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  )

  assert.eq(
    instantiate(`
      (module
        (func (param funcref) (result i32)
          (ref.test (ref nofunc) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (ref.null func))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (start 1)
        (func (param externref) (result nullexternref)
          (ref.cast (ref noextern) (local.get 0)))
        (func
          (call 0 (ref.null extern))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  )

  assert.eq(
    instantiate(`
      (module
        (func (param externref) (result i32)
          (ref.test (ref noextern) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (ref.null extern))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (struct (field i32)))
        (start 1)
        (func (param structref) (result nullref)
          (ref.cast (ref none) (local.get 0)))
        (func
          (call 0 (struct.new 0 (i32.const 42)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (type (struct (field i32)))
        (func (param structref) (result i32)
          (ref.test (ref none) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (struct.new 0 (i32.const 42)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => instantiate(`
      (module
        (start 1)
        (func (param i31ref) (result nullref)
          (ref.cast (ref none) (local.get 0)))
        (func
          (call 0 (ref.i31 (i32.const 42)))
          drop))
    `),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type"
  );

  assert.eq(
    instantiate(`
      (module
        (func (param i31ref) (result i32)
          (ref.test (ref none) (local.get 0)))
        (func (export "f") (result i32)
          (call 0 (ref.i31 (i32.const 42)))))
    `).exports.f(),
    0
  );

  assert.throws(
    () => compile(`
      (module
        (type (func (result i32)))
        (elem declare funcref (ref.func 0))
        (func (type 0) (i32.const 42))
        (func (param funcref)
          (ref.cast (ref none) (local.get 0))
          drop))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.cast to type (ref null func) expected a subtype of anyref, in function at index 1 (evaluating 'new WebAssembly.Module(binary)')"
  );
}

function testLargeIndexCasts() {
  {
    let typedefs = "", casts = "";
    for (var i = 0; i < 500; i++) {
      typedefs += `(type (struct))\n`;
      casts += `(ref.cast (ref ${i}) (struct.new ${i})) drop\n`
    }
    instantiate(`
      (module
        ${typedefs}
        (start 0)
        (func ${casts}))
    `);
  }
}

testValidation();
testBasicCasts();
testI31Casts();
testFunctionCasts();
testArrayCasts();
testStructCasts();
testSubtypeCasts();
testEqCasts();
testAnyCasts();
testNullCasts();
testLargeIndexCasts();
