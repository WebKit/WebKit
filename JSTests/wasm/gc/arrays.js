//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function testArrayDeclaration() {
  instantiate(`
    (module
      (type (array i32))
    )
  `);

  instantiate(`
    (module
      (type (array (mut i32)))
    )
  `);

  /*
   * invalid element type
   * (module
   *   (type (array <invalid>))
   * )
   */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x5e\xff\x02")),
    WebAssembly.CompileError,
    "Module doesn't parse at byte 17: can't get array's element Type"
  )

  /*
   * invalid mut value 0x02
   * (module
   *   (type (array (<invalid> i32)))
   * )
   */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x5e\x7f\x02")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 18: invalid array mutability: 0x02"
  )

  instantiate(`
    (module
      (type (array i32))
      (func (result (ref null 0)) (ref.null 0))
    )
  `);

  instantiate(`
    (module
      (type (array i32))
      (func (result arrayref) (ref.null 0))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (func (result (ref null 0)) (ref.null array))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. Arrayref is not a (I32, mutable), in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (func (result funcref) (ref.null 0))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (I32, mutable) is not a Funcref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );
}

function testArrayJS() {
  // JS API behavior not specified yet, import/export error for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array i64))
          (func (export "f") (result (ref null 0))
            (ref.null 0))
        )
      `);
      m.exports.f();
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array externref))
          (func (export "f") (param (ref null 0)))
        )
      `);
      m.exports.f(null);
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array f32))
          (import "m" "f" (func (param (ref null 0))))
          (func (export "g") (call 0 (ref.null 0)))
        )
      `, { m: { f: (x) => { return; } } });
      m.exports.g();
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array i32))
          (import "m" "f" (func (result (ref null 0))))
          (func (export "g") (call 0) drop)
        )
      `, { m: { f: (x) => { return null; } } });
      m.exports.g();
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  // JS API behavior not specified yet, setting global errors for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array i32))
          (global (export "g") (mut (ref null 0)) (ref.null 0))
        )
      `);
      m.exports.g.value = 42;
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )
}

function testArrayNew() {
  instantiate(`
    (module
      (type (array i32))
      (func (result (ref 0))
        (array.new_canon 0 (i32.const 42) (i32.const 5)))
    )
  `);
}

function testArrayNewDefault() {
  instantiate(`
    (module
      (type (array i32))
      (func (result (ref 0))
        (array.new_canon_default 0 (i32.const 5)))
    )
  `);
}

function testArrayGet() {
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0) (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), 0);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (array.new_canon_default 0 (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), 0);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 42) (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), 42);
  }

  {
    let m = instantiate(`
      (module
        (type (array i64))
        (func (export "f") (result i64)
          (array.new_canon 0 (i64.const 0x0eee_ffff_ffff_ffff) (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), 0x0eeeffffffffffffn);
  }

  {
    let m = instantiate(`
      (module
        (type (array f32))
        (func (export "f") (result f32)
          (array.new_canon 0 (f32.const 0.25) (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), 0.25);
  }

  {
    let m = instantiate(`
      (module
        (type (array f64))
        (func (export "f") (result f64)
          (array.new_canon 0 (f64.const 1e39) (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), 1e39);
  }

  {
    let m = instantiate(`
      (module
        (type (array externref))
        (func (export "f") (param externref) (result externref)
          (array.new_canon 0 (local.get 0) (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    let sym = Symbol();
    assert.eq(m.exports.f(sym), sym);
    assert.eq(m.exports.f({ x: 1, y: 2 }).x, 1);
  }

  {
    let m = instantiate(`
      (module
        (type (array externref))
        (func (export "f") (result externref)
          (array.new_canon_default 0 (i32.const 5))
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.eq(m.exports.f(), null);
  }

  {
    let m = instantiate(`
      (module
        (type (array externref))
        (import "m" "g" (func $g))
        (global $a (mut (ref null 0)) (ref.null 0))
        (func (export "new") (param externref)
          (array.new_canon 0 (local.get 0) (i32.const 5))
          (call $g)
          (global.set $a))
        (func (export "get") (result externref)
          (call $g)
          (global.get $a)
          (i32.const 3)
          (array.get 0))
      )`,
      { m: { g: () => { fullGC(); } } }
    );
    m.exports.new({ x: 1, y: 2 });
    fullGC();
    for (let i = 0; i < 1000; i++) { let o = { x: i }; }
    assert.eq(m.exports.get().x, 1);
    assert.eq(m.exports.get().y, 2);
  }

  // Should trap on null array.
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (ref.null 0)
          (i32.const 3)
          (array.get 0))
      )
    `);
    assert.throws(
      () => { m.exports.f(); },
      WebAssembly.RuntimeError,
      "array.get to a null reference"
    );
  }

  // Should trap on index out of bounds.
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 42) (i32.const 5))
          (i32.const 18)
          (array.get 0))
      )
    `);
    assert.throws(
      () => { m.exports.f(); },
      WebAssembly.RuntimeError,
      "Out of bounds array.get"
    );
  }
}

function testArraySet() {
  {
    let m = instantiate(`
      (module
        (type (array (mut i32)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init")
          (global.set 0 (array.new_canon 0 (i32.const 42) (i32.const 5)))
          (array.set 0 (global.get 0) (i32.const 3) (i32.const 84)))
        (func (export "get") (param i32) (result i32)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    m.exports.init();
    assert.eq(m.exports.get(0), 42);
    assert.eq(m.exports.get(3), 84);
  }

  {
    let m = instantiate(`
      (module
        (type (array (mut i64)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init")
          (global.set 0 (array.new_canon 0 (i64.const 0x0eee_ffff_ffff_ffff) (i32.const 5)))
          (array.set 0 (global.get 0) (i32.const 3) (i64.const 0x0abc_ffff_ffff_ffff)))
        (func (export "get") (param i32) (result i64)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    m.exports.init();
    assert.eq(m.exports.get(0), 0x0eeeffffffffffffn);
    assert.eq(m.exports.get(3), 0x0abcffffffffffffn);
  }

  {
    let m = instantiate(`
      (module
        (type (array (mut f32)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init")
          (global.set 0 (array.new_canon 0 (f32.const 0.25) (i32.const 5)))
          (array.set 0 (global.get 0) (i32.const 3) (f32.const 0.125)))
        (func (export "get") (param i32) (result f32)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    m.exports.init();
    assert.eq(m.exports.get(0), 0.25);
    assert.eq(m.exports.get(3), 0.125);
  }

  {
    let m = instantiate(`
      (module
        (type (array (mut f64)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init")
          (global.set 0 (array.new_canon 0 (f64.const 1e39) (i32.const 5)))
          (array.set 0 (global.get 0) (i32.const 3) (f64.const 1e38)))
        (func (export "get") (param i32) (result f64)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    m.exports.init();
    assert.eq(m.exports.get(0), 1e39);
    assert.eq(m.exports.get(3), 1e38);
  }

  {
    let m = instantiate(`
      (module
        (type (array (mut externref)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init") (param externref externref)
          (global.set 0 (array.new_canon 0 (local.get 0) (i32.const 5)))
          (array.set 0 (global.get 0) (i32.const 3) (local.get 1)))
        (func (export "get") (param i32) (result externref)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    let sym = Symbol();
    m.exports.init(sym, { x: 1, y: 2 });
    assert.eq(m.exports.get(0), sym);
    assert.eq(m.exports.get(3).x, 1);
    assert.eq(m.exports.get(3).y, 2);
  }

  {
    assert.throws(
      () => compile(`
        (module
          (type (array i32))
          (func (export "f") (result i32)
            (array.set 0 (ref.null 0) (i32.const 3) (i32.const 42)))
        )
      `),
      WebAssembly.CompileError,
      "array.set index 0 does not reference a mutable array definition"
    );
  }

  // Should trap on null array.
  {
    let m = instantiate(`
      (module
        (type (array (mut i32)))
        (func (export "f")
          (array.set 0 (ref.null 0) (i32.const 3) (i32.const 42)))
      )
    `);
    assert.throws(
      () => { m.exports.f(); },
      WebAssembly.RuntimeError,
      "array.set to a null reference"
    );
  }

  // Should trap on index out of bounds.
  {
    let m = instantiate(`
      (module
        (type (array (mut i32)))
        (func (export "f")
          (array.new_canon 0 (i32.const 42) (i32.const 5))
          (i32.const 18)
          (i32.const 84)
          (array.set 0))
      )
    `);
    assert.throws(
      () => { m.exports.f(); },
      WebAssembly.RuntimeError,
      "Out of bounds array.set"
    );
  }
}

function testArrayLen() {
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 42) (i32.const 5))
          (array.len))
      )
    `);
    assert.eq(m.exports.f(), 5);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 42) (i32.const 0))
          (array.len))
      )
    `);
    assert.eq(m.exports.f(), 0);
  }

  // Should trap on null array.
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result i32)
          (ref.null 0)
          (array.len))
      )
    `);
    assert.throws(
      () => { m.exports.f(); },
      WebAssembly.RuntimeError,
      "array.len to a null reference"
    );
  }
}

testArrayDeclaration();
testArrayJS();
testArrayNew();
testArrayNewDefault();
testArrayGet();
testArraySet();
testArrayLen();
