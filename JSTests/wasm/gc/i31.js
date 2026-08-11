import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testI31Type() {
  compile(`
    (module
       (type (func (param i31ref) (result (ref i31))))
    )
  `);

  compile(`
    (module
      (global i31ref (ref.null i31))
    )
  `);
}

function testI31New() {
  assert.throws(
    () => instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.i31 (f32.const 42.42)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.i31 value to type F32 expected I32, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  )

  assert.throws(
    () => instantiate(`
      (module
        (func (export "f") (result i32)
          (ref.i31 (i64.const 42)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.i31 value to type I64 expected I32, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  )

  // Use i31 in global and also export to JS via global.
  {
    let m = instantiate(`
      (module
        (global (export "g") (mut i31ref) (ref.null i31))
        (func (export "f")
          (global.set 0 (ref.i31 (i32.const 42))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++) {
      m.exports.f();
      assert.eq(m.exports.g.value, 42);
    }
  }

  // Test export to JS.
  {
    let m = instantiate(`
      (module
        (func (export "f") (result i31ref)
          (ref.i31 (i32.const 42)))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), 42);
  }

  // Export to JS via import.
  {
    let m = instantiate(`
      (module
        (import "m" "f" (func (param i31ref)))
        (func (export "g")
          (call 0 (ref.i31 (i32.const 42))))
      )`,
      { m: { f: (i31) => assert.eq(i31, 42) } }
    );
    for (let i = 0; i < testLoopCount; i++)
      m.exports.g();
  }
}

function testI31Get() {
  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (ref.i31 (i32.const 42))))
      )
    `);
    assert.eq(m.exports.f(), 42);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_u (ref.i31 (i32.const 42))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), 42);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (ref.i31 (i32.const 0x4000_0000))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), -0x40000000);
  }

  {
    let m = instantiate(
      `(module
         (func (export "f") (result i32)
           (i31.get_u (ref.i31 (i32.const 0x4000_0000))))
       )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), 0x40000000);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (ref.i31 (i32.const 0xaaaa_aaaa))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), 0x2aaaaaaa);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_u (ref.i31 (i32.const 0xaaaa_aaaa))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), 0x2aaaaaaa);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (ref.i31 (i32.const -1))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), -1);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_u (ref.i31 (i32.const -1))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      assert.eq(m.exports.f(), 0x7fffffff);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param $arg i32) (result i32)
          (i31.get_u (ref.i31 (local.get $arg))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++) {
      assert.eq(m.exports.f(0x7fffffff), 0x7fffffff);
      assert.eq(m.exports.f(0x2aaaaaaa), 0x2aaaaaaa);
      assert.eq(m.exports.f(0x40000000), 0x40000000);
      assert.eq(m.exports.f(42), 42);
    }
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param $arg i32) (result i32)
          (i31.get_s (ref.i31 (local.get $arg))))
      )
    `);
    for (let i = 0; i < testLoopCount; i++) {
      assert.eq(m.exports.f(0x7fffffff), -1);
      assert.eq(m.exports.f(0x2aaaaaaa), 0x2aaaaaaa);
      assert.eq(m.exports.f(0x40000000), -0x40000000);
      assert.eq(m.exports.f(42), 42);
    }
  }

  assert.throws(
    () => instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (ref.null extern)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: i31.get_s ref to type (ref null extern) expected I31ref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (func (export "f") (result i32)
            (i31.get_s (ref.null i31)))
        )
      `);
      m.exports.f();
    },
    WebAssembly.RuntimeError,
    "i31.get_<sx> to a null reference"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (func (export "f") (result i32)
            (i31.get_u (ref.null i31)))
        )
      `);
      m.exports.f();
    },
    WebAssembly.RuntimeError,
    "i31.get_<sx> to a null reference"
  )
}

function testI31JS() {
  {
    let m = instantiate(`
      (module
        (func (export "f") (param i31ref) (result i32)
          (i31.get_u (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(42), 42);
    assert.throws(
      () => m.exports.f(m.exports.f),
      TypeError,
      "Argument value did not match the reference type"
    );
    assert.throws(
      () => m.exports.f(null),
      WebAssembly.RuntimeError,
      "i31.get_<sx> to a null reference"
    );
  }

  // JS API behavior not specified yet, i31 global errors for now.
  assert.throws(
    () => { return new WebAssembly.Global({ type: "i31ref" }, 42) },
    TypeError,
    "WebAssembly.Global expects its 'value' field to be the string"
  )

  {
    let m = instantiate(`
      (module
        (global (export "g") (mut i31ref) (ref.null i31))
      )
    `);
    for (let i = 0; i < testLoopCount; i++)
      m.exports.g.value = 42;
  }
}

function testI31Table() {
  {
    let m = instantiate(`
      (module
        (table 10 (ref null i31))
        (func (export "set") (param i32) (result)
          (table.set (local.get 0) (ref.i31 (i32.const 42))))
        (func (export "get") (param i32) (result i32)
          (i31.get_s (table.get (local.get 0)))))
    `);
    for (let i = 0; i < testLoopCount; i++) {
      m.exports.set(3);
      assert.eq(m.exports.get(3), 42);
      m.exports.set(0);
    }
    assert.throws(() => m.exports.get(2), WebAssembly.RuntimeError, "i31.get_<sx> to a null");
  }

  // Invalid non-defaultable table type.
  assert.throws(
    () => compile(`
      (module
        (table 0 (ref i31)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 17: Table's type must be defaultable (evaluating 'new WebAssembly.Module(binary)')"
  )

  // Test JS API.
  {
    const m = instantiate(`
      (module
        (table (export "t") 10 (ref null i31))
        (start 0)
        (func
          (table.fill (i32.const 0) (ref.i31 (i32.const 42)) (i32.const 10))))
    `);
    const m2 = instantiate(`
      (module
        (type (array i32))
        (func (export "makeArray") (result (ref 0)) (array.new_default 0 (i32.const 5))))
    `);
    assert.eq(m.exports.t.get(0), 42);
    assert.throws(
      () => m.exports.t.set(0, "foo"),
      TypeError,
      "Argument value did not match the reference type"
    );
    assert.throws(
      () => m.exports.t.set(0, m2.exports.makeArray()),
      TypeError,
      "Argument value did not match the reference type"
    );
    assert.throws(
      () => m.exports.t.set(0, 8e99),
      TypeError,
      "Argument value did not match the reference type"
    );
    for (let i = 0; i < testLoopCount; i++) {
      m.exports.t.set(0, 3);
      assert.eq(m.exports.t.get(0), 3);
      m.exports.t.set(0, null);
      assert.eq(m.exports.t.get(0), null);
    }
  }
}

testI31Type();
testI31New();
testI31Get();
testI31JS();
testI31Table();
