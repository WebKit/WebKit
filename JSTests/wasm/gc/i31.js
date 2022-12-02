//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

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
          (i31.new (f32.const 42.42)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: i31.new value to type F32 expected I32, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  )

  // Use i31 in global and also export to JS via global.
  {
    let m = instantiate(`
      (module
        (global (export "g") (mut i31ref) (ref.null i31))
        (func (export "f")
          (global.set 0 (i31.new (i32.const 42))))
      )
    `);
    m.exports.f();
    assert.eq(m.exports.g.value, 42);
  }

  // Test export to JS.
  {
    let m = instantiate(`
      (module
        (func (export "f") (result i31ref)
          (i31.new (i32.const 42)))
      )
    `);
    assert.eq(m.exports.f(), 42);
  }

  // Export to JS via import.
  {
    let m = instantiate(`
      (module
        (import "m" "f" (func (param i31ref)))
        (func (export "g")
          (call 0 (i31.new (i32.const 42))))
      )`,
      { m: { f: (i31) => assert.eq(i31, 42) } }
    );
    m.exports.g();
  }
}

function testI31Get() {
  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (i31.new (i32.const 42))))
      )
    `);
    assert.eq(m.exports.f(), 42);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_u (i31.new (i32.const 42))))
      )
    `);
    assert.eq(m.exports.f(), 42);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (i31.new (i32.const 0x4000_0000))))
      )
    `);
    assert.eq(m.exports.f(), -0x40000000);
  }

  {
    let m = instantiate(
      `(module
         (func (export "f") (result i32)
           (i31.get_u (i31.new (i32.const 0x4000_0000))))
       )
    `);
    assert.eq(m.exports.f(), 0x40000000);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (i31.new (i32.const 0xaaaa_aaaa))))
      )
    `);
    assert.eq(m.exports.f(), 0x2aaaaaaa);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_u (i31.new (i32.const 0xaaaa_aaaa))))
      )
    `);
    assert.eq(m.exports.f(), 0x2aaaaaaa);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (i31.new (i32.const -1))))
      )
    `);
    assert.eq(m.exports.f(), -1);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_u (i31.new (i32.const -1))))
      )
    `);
    assert.eq(m.exports.f(), 0x7fffffff);
  }

  assert.throws(
    () => instantiate(`
      (module
        (func (export "f") (result i32)
          (i31.get_s (ref.null extern)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: i31.get_s ref to type Externref expected I31ref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
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
  // JS API behavior not specified yet, import errors for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (func (export "f") (param i31ref) (result i32)
            (i31.get_u (local.get 0)))
        )
      `);
      m.exports.f(42);
    },
    WebAssembly.RuntimeError,
    "I31ref import from JS currently unsupported"
  )

  // JS API behavior not specified yet, i31 global errors for now.
  assert.throws(
    () => { return new WebAssembly.Global({ type: "i31ref" }, 42) },
    TypeError,
    "WebAssembly.Global expects its 'value' field to be the string"
  )

  // JS API behavior not specified yet, setting global errors for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (global (export "g") (mut i31ref) (ref.null i31))
        )
      `);
      m.exports.g.value = 42;
    },
    WebAssembly.RuntimeError,
    "I31ref import from JS currently unsupported"
  )
}

testI31Type();
testI31New();
testI31Get();
testI31JS();
