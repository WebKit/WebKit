//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--useWebAssemblyExtendedConstantExpressions=true")

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

function runWasmGCObjectTests(obj) {
  assert.eq(obj.foo, undefined);
  assert.eq(obj["foo"], undefined);
  assert.eq(obj[0], undefined);
  assert.eq(Object.getOwnPropertyNames(obj).length, 0);
  assert.eq(Object.hasOwn(obj, "foo"), false);
  assert.eq(Object.prototype.hasOwnProperty.apply(obj, ["foo"]), false);
  assert.eq("foo" in obj, false);
  assert.eq(Object.getOwnPropertyDescriptor(obj, "foo"), undefined);
  assert.throws(
    () => { obj.foo = 42; },
    TypeError,
    "Cannot set property for WebAssembly GC object"
  );
  assert.throws(
    () => { obj[0] = 5; },
    TypeError,
    "Cannot set property for WebAssembly GC object"
  );
  assert.throws(
    () => { delete obj.foo; },
    TypeError,
    "Cannot delete property for WebAssembly GC object"
  );
  assert.throws(
    () => { delete obj[0]; },
    TypeError,
    "Cannot delete property for WebAssembly GC object"
  );
  assert.throws(
    () => Object.defineProperty(obj, "foo", { value: 42 }),
    TypeError,
    "Cannot define property for WebAssembly GC object"
  )
  assert.eq(Object.getPrototypeOf(obj), null);
  assert.throws(
    () => Object.setPrototypeOf(obj, {}),
    TypeError,
    "Cannot set prototype of WebAssembly GC object"
  )
  assert.eq(Object.isExtensible(obj), false);
  assert.throws(
    () => Object.preventExtensions(obj),
    TypeError,
    "Cannot run preventExtensions operation on WebAssembly GC object"
  )
  assert.throws(
    () => Object.seal(obj),
    TypeError,
    "Cannot run preventExtensions operation on WebAssembly GC object"
  )
}

function testArray() {
  let m = instantiate(`
    (module
      (type (array i32))
      (func (export "make") (result (ref 0))
        (array.new 0 (i32.const 42) (i32.const 5)))
    )
  `);
  const arr = m.exports.make();
  runWasmGCObjectTests(arr, m.exports.make);
}

function testStruct() {
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "make") (result (ref 0))
        (struct.new 0 (i32.const 42)))
    )
  `);
  const struct = m.exports.make();
  runWasmGCObjectTests(struct, m.exports.make);
}

function testI31() {
  {
    let m = instantiate(`
      (module
        (func (export "f") (param i31ref) (result i32)
          (i31.get_s (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(2.0), 2);
    assert.eq(m.exports.f(2 ** 30 - 1), 2 ** 30 - 1);
    assert.eq(m.exports.f(2.0 ** 30 - 1), 2 ** 30 - 1);
    assert.eq(m.exports.f(-(2.0 ** 30)), -(2 ** 30));
    assert.throws(
      () => m.exports.f(2.3),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.f(2n),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.f(2 ** 30),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.f(-(2 ** 30) - 1),
      TypeError,
      "Argument value did not match reference type"
    );
  }
}

function testRoundtrips() {
  {
    let m = instantiate(`
      (module
        (type (struct (field i32)))
        (func (export "make") (result (ref 0))
          (struct.new 0 (i32.const 42)))
        (func (export "f") (param (ref 0)) (result i32)
          (struct.get 0 0 (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(m.exports.make()), 42);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "make") (result (ref 0))
          (array.new 0 (i32.const 42) (i32.const 5)))
        (func (export "f") (param (ref 0) i32) (result i32)
          (array.get 0 (local.get 0) (local.get 1)))
      )
    `);
    assert.eq(m.exports.f(m.exports.make(), 3), 42);
  }

  {
    let m = instantiate(`
      (module
        (func (export "make") (param i32) (result i31ref)
          (ref.i31 (local.get 0)))
        (func (export "f") (param i31ref) (result i32)
          (i31.get_s (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(m.exports.make(42)), 42);
    assert.eq(m.exports.f(m.exports.make(42.0)), 42);
    assert.eq(m.exports.f(m.exports.make(42.3)), 42);
    assert.eq(m.exports.f(m.exports.make(2 ** 30 - 1)), 2 ** 30 - 1);
  }

  // Ensure eq, any, etc. can also be used to roundtrip values
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "make") (result (ref eq))
          (array.new 0 (i32.const 42) (i32.const 5)))
        (func (export "f") (param (ref eq) i32) (result i32)
          (array.get 0 (ref.cast (ref 0) (local.get 0)) (local.get 1)))
      )
    `);
    assert.eq(m.exports.f(m.exports.make(), 3), 42);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "make") (result (ref any))
          (array.new 0 (i32.const 42) (i32.const 5)))
        (func (export "f") (param (ref any) i32) (result i32)
          (array.get 0 (ref.cast (ref 0) (local.get 0)) (local.get 1)))
      )
    `);
    assert.eq(m.exports.f(m.exports.make(), 3), 42);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "make") (result (ref array))
          (array.new 0 (i32.const 42) (i32.const 5)))
        (func (export "f") (param (ref array) i32) (result i32)
          (array.get 0 (ref.cast (ref 0) (local.get 0)) (local.get 1)))
      )
    `);
    assert.eq(m.exports.f(m.exports.make(), 3), 42);
  }

  {
    let m = instantiate(`
      (module
        (type (struct (field i32)))
        (func (export "make") (result (ref struct))
          (struct.new 0 (i32.const 42)))
        (func (export "f") (param (ref struct)) (result i32)
          (struct.get 0 0 (ref.cast (ref 0) (local.get 0))))
      )
    `);
    assert.eq(m.exports.f(m.exports.make()), 42);
  }
}

function testCastFailure() {
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (param (ref 0) i32) (result i32)
          (array.get 0 (local.get 0) (local.get 1)))
      )
    `);
    assert.throws(
      () => m.exports.f({}, 3),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param (ref eq)) (result))
      )
    `);
    assert.throws(
      () => m.exports.f({}),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (global (export "g") (ref 0) (struct.new 0))
        (func (export "f") (param (ref func)) (result))
      )
    `);
    assert.throws(
      () => m.exports.f(m.exports.g.value),
      TypeError,
      "Funcref must be an exported wasm function"
    );
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (type (func))
        (global (export "g") (ref 0) (struct.new 0))
        (func (export "f") (param (ref 1)) (result))
      )
    `);
    assert.throws(
      () => m.exports.f(m.exports.g.value),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param (ref struct)) (result))
      )
    `);
    assert.throws(
      () => m.exports.f({}),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param (ref array)) (result))
      )
    `);
    assert.throws(
      () => m.exports.f({}),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param (ref any)) (result))
      )
    `);
    m.exports.f(m.exports.f);
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param i31ref) (result))
      )
    `);
    assert.throws(
      () => m.exports.f({}),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.f(2 ** 31),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct (field i32))))
        (type (sub 0 (struct (field i32 i64))))
        (global (export "s0") (ref 0) (struct.new 0 (i32.const 42)))
        (global (export "s1") (ref 1) (struct.new 1 (i32.const 42)))
        (global (export "s2") (ref 2) (struct.new 2 (i32.const 42) (i64.const 84)))
        (func (export "f") (param (ref 0)) (result))
        (func (export "g") (param (ref 1)) (result))
        (func (export "h") (param (ref 2)) (result))
      )
    `);
    m.exports.f(m.exports.s0.value);
    m.exports.f(m.exports.s1.value);
    m.exports.f(m.exports.s2.value);
    m.exports.g(m.exports.s1.value);
    m.exports.h(m.exports.s2.value);
    assert.throws(
      () => m.exports.g(m.exports.s0.value),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.g(m.exports.s2.value),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.h(m.exports.s0.value),
      TypeError,
      "Argument value did not match reference type"
    );
    assert.throws(
      () => m.exports.h(m.exports.s1.value),
      TypeError,
      "Argument value did not match reference type"
    );
  }
}

function testGlobal() {
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (global (export "g") (mut (ref 0))
          (array.new 0 (i32.const 42) (i32.const 10)))
        (func (export "f") (param (ref 0) i32) (result i32)
          (array.get 0 (local.get 0) (local.get 1)))
      )
    `);
    assert.eq(m.exports.f(m.exports.g.value, 0), 42);
    assert.throws(
      () => { m.exports.g.value = { } },
      TypeError,
      "Argument value did not match reference type"
    )
  }

  {
    let m = instantiate(`
      (module
        (type (struct (field i64)))
        (global (export "g") (mut (ref 0))
          (struct.new 0 (i64.const 42)))
        (func (export "f") (param (ref 0)) (result i64)
          (struct.get 0 0 (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(m.exports.g.value), 42n);
    assert.throws(
      () => { m.exports.g.value = { } },
      TypeError,
      "Argument value did not match reference type"
    )
  }

  {
    let m = instantiate(`
      (module
        (type (struct (field f32)))
        (global (export "g") (mut (ref eq))
          (struct.new 0 (f32.const 0.25)))
        (func (export "f") (param (ref eq)) (result f32)
          (struct.get 0 0 (ref.cast (ref 0) (local.get 0))))
      )
    `);
    assert.eq(m.exports.f(m.exports.g.value), 0.25);
    assert.throws(
      () => { m.exports.g.value = { } },
      TypeError,
      "Argument value did not match reference type"
    )
  }

  {
    let m = instantiate(`
      (module
        (type (struct (field f64)))
        (global (export "g") (mut (ref any))
          (struct.new 0 (f64.const 0.25)))
        (func (export "f") (param (ref any)) (result f64)
          (struct.get 0 0 (ref.cast (ref 0) (local.get 0))))
      )
    `);
    assert.eq(m.exports.f(m.exports.g.value), 0.25);
    // Test that a JS hostref can inhabit (ref any)
    m.exports.g.value = { };
    assert.throws(
      () => { m.exports.f(m.exports.g.value) },
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    )
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (mut (ref i31))
          (ref.i31 (i32.const 42)))
        (func (export "f") (param (ref i31)) (result i32)
          (i31.get_s (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(m.exports.g.value), 42);
  }

  {
    let m1 = instantiate(`
      (module
        (func (export "f"))
      )
    `);
    instantiate(`
      (module
        (global (import "m" "f") (ref any))
      )
    `, { m: { f: m1.exports.f } });
  }

  // Test portable globals, with mutation.
  {
    let m = instantiate(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct (field i32))))
        (type (sub 0 (struct (field i32 i64))))
        (global (export "g01") (mut (ref 0)) (struct.new 0 (i32.const 42)))
        (global (export "g02") (mut (ref null 0)) (ref.null 0))
        (global (export "g1") (mut (ref null 1)) (struct.new 1 (i32.const 42)))
        (global (export "g2") (mut (ref null 2)) (struct.new 2 (i32.const 42) (i64.const 84)))
      )
    `);
    m.exports.g01.value = m.exports.g1.value;
    m.exports.g02.value = m.exports.g2.value;
    m.exports.g1.value = null;
    m.exports.g2.value = null;
  }

  // Test type descriptor
  {
    let m = instantiate(`
      (module
        (type (struct (field f32)))
        (global (export "g") (mut (ref null 0)) (struct.new 0 (f32.const 0.25))))
    `);
    assert.throws(
      () => m.exports.g.type(),
      TypeError,
      "WebAssembly.Global.prototype.type unable to produce type descriptor for the given global"
    )
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (ref func) (ref.func 0))
        (func)
      )
    `);
    assert.throws(
      () => m.exports.g.type(),
      TypeError,
      "WebAssembly.Global.prototype.type unable to produce type descriptor for the given global"
    )
  }
}

function testTable() {
  {
    let m = instantiate(`
      (module
        (type (sub (array i32)))
        (type (sub 0 (array i32)))
        (global (export "g") (ref 0) (array.new 1 (i32.const 42) (i32.const 10)))
        (table (export "t") 10 funcref)
        (elem declare funcref (ref.func 1))
        (func (table.set (i32.const 0) (ref.func 1)))
        (func (param (ref 0) i32) (result i32)
          (array.get 0 (local.get 0) (local.get 1)))
        (start 0)
      )
    `);
    assert.eq(m.exports.t.get(0)(m.exports.g.value), 42);
  }

  {
    let m1 = instantiate(`
      (module
        (func (export "f"))
      )
    `);
    let m2 = instantiate(`
      (module
        (table (export "t") 10 (ref null any))
      )
    `);
    m2.exports.t.set(0, m1.exports.f);
  }

  {
    let m1 = instantiate(`
      (module
        (table (export "t") 10 (ref null func))
      )
    `);
    assert.throws(
      () => m1.exports.t.set(0, 5),
      TypeError,
      "WebAssembly.Table.prototype.set expects the second argument to be null or an instance of WebAssembly.Function"
    )
  }

  // Test type descriptor
  {
    let m = instantiate(`
      (module
        (type (struct (field f32)))
        (table (export "t") 10 (ref null 0))
      )
    `);
    assert.throws(
      () => m.exports.t.type(),
      TypeError,
      "WebAssembly.Table.prototype.type unable to produce type descriptor for the given table"
    )
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (table (export "t") 10 (ref 0) (struct.new 0))
      )
    `);
    assert.throws(
      () => m.exports.t.type(),
      TypeError,
      "WebAssembly.Table.prototype.type unable to produce type descriptor for the given table"
    )
  }

  {
    let m = instantiate(`
      (module
        (table (export "t") 10 (ref func) (ref.func 0))
        (func)
      )
    `);
    assert.throws(
      () => m.exports.t.type(),
      TypeError,
      "WebAssembly.Table.prototype.type unable to produce type descriptor for the given table"
    )
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (table (export "t") 10 (ref 0) (struct.new 0))
      )
    `);
    assert.throws(
      () => m.exports.t.set(1),
      TypeError,
      "WebAssembly.Table.prototype.set requires the second argument for non-defaultable table type"
    );
    assert.throws(
      () => m.exports.t.grow(1),
      TypeError,
      "WebAssembly.Table.prototype.grow requires the second argument for non-defaultable table type"
    );
  }

  {
    let m = instantiate(`
      (module
        (table (export "t") 10 (ref func) (ref.func 0))
        (func)
      )
    `);
    assert.throws(
      () => m.exports.t.set(1),
      TypeError,
      "WebAssembly.Table.prototype.set requires the second argument for non-defaultable table type"
    );
    assert.throws(
      () => m.exports.t.grow(1),
      TypeError,
      "WebAssembly.Table.prototype.grow requires the second argument for non-defaultable table type"
    );
  }

  {
    let m = instantiate(`
      (module
        (table (export "t") 10 (ref null i31) (ref.i31 (i32.const 42)))
      )
    `);
    let origSize = m.exports.t.length;
    for (let i = 0; i < origSize; i++)
      assert.eq(m.exports.t.get(i), 42);
    m.exports.t.grow(5);
    for (let i = 0; i < origSize; i++)
      assert.eq(m.exports.t.get(i), 42);
    for (let i = origSize; i < origSize + 5; i++)
      assert.eq(m.exports.t.get(i), null);
    m.exports.t.set(1);
    assert.eq(m.exports.t.get(1), null);
  }
}

function testImport() {
  {
    let m1 = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (param (ref 0) i32) (result i32)
          (array.get 0 (local.get 0) (local.get 1)))
      )
    `);
    let m2 = instantiate(`
      (module
        (type (array i64))
        (import "m" "f" (func (param (ref 0) i32) (result i64)))
        (global (ref 0) (array.new 0 (i64.const 42) (i32.const 10)))
        (func (export "f") (result i64)
          (call 0 (global.get 0) (i32.const 0)))
      )
    `, { m: { f : (arr, idx) => { m1.exports.f(arr, idx) } } });
    assert.throws(
      () => m2.exports.f(),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  // Ensure import return types are checked
  {
    let m = instantiate(
      `
        (module
          (func (import "m" "f") (result i31ref))
          (func (export "g") (result i31ref)
            (call 0))
        )
      `,
      { m: { f: () => { return "foo"; } } });
    assert.throws(
      () => m.exports.g(),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(
      `
        (module
          (func (import "m" "f") (result i31ref))
          (func (export "g") (result i31ref)
            (call 0))
        )
      `,
      { m: { f: () => { return 42; } } });
    assert.eq(m.exports.g(), 42);
  }

  {
    let m1 = instantiate(`
        (module
          (type (func))
          (func (export "f"))
        )
    `);
    let m2 = instantiate(
      `
        (module
          (type (func))
          (func (import "m" "f") (result (ref 0)))
          (func (export "g") (result (ref 0))
            (call 0))
        )
      `,
      { m: { f: () => { return m1.exports.f; } } });
    assert.isFunction(m2.exports.g());
  }

  {
    let m = instantiate(
      `
        (module
          (type (struct))
          (func (import "m" "f") (result (ref null 0)))
          (func (export "g") (result (ref null 0))
            (call 0))
        )
      `,
      { m: { f: () => { return null; } } });
    assert.eq(m.exports.g(), null);
  }

  {
    let m = instantiate(
      `
        (module
          (type (array i32))
          (func (import "m" "f") (result (ref 0)))
          (func (export "g") (result (ref 0))
            (call 0))
        )
      `,
      { m: { f: () => { return "foo"; } } });
    assert.throws(
      () => m.exports.g(),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  {
    let m = instantiate(
      `
        (module
          (type (struct))
          (func (import "m" "f") (result (ref 0)))
          (func (export "g") (result (ref 0))
            (call 0))
        )
      `,
      { m: { f: () => { return "foo"; } } });
    assert.throws(
      () => m.exports.g(),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  // Ensure multi-value return is checked
  {
    let m = instantiate(
      `
        (module
          (func (import "m" "f") (result i32 i31ref))
          (func (export "g") (result i32 i31ref)
            (call 0))
        )
      `,
      { m: { f: () => { return [5, "foo"]; } } });
    assert.throws(
      () => m.exports.g(),
      TypeError,
      "Argument value did not match reference type"
    );
  }

  // Test direct global imports.
  {
    function doImport(type, val) {
      instantiate(`
        (module
          (type (struct))
          (type (array i32))
          (global (import "m" "g") (ref ${type}))
        )
      `,
      { m: { g: val } });
    }
    function castError(type, val) {
      assert.throws(
        () => { doImport(type, val); },
        WebAssembly.LinkError,
        "Argument value did not match the reference type"
      );
    }
    let m = instantiate(`
      (module
        (type (struct))
        (type (array i32))
        (func (export "makeS") (result anyref) (struct.new 0))
        (func (export "makeA") (result anyref) (array.new_default 1 (i32.const 10)))
      )
    `);
    doImport("any", "foo");
    doImport("i31", 2 ** 30 - 1);
    doImport("i31", -(2 ** 30));
    castError("i31", 2.3);
    doImport("any", "foo");
    doImport("struct", m.exports.makeS());
    doImport("array", m.exports.makeA());
    doImport("0", m.exports.makeS());
    doImport("1", m.exports.makeA());
    castError("0", m.exports.makeA());
  }
}

function testTag() {
  {
    /*
     * (module
     *   (type (struct (field f32)))
     *   (tag (export "t") (param (ref null 0)))
     * )
     */
    let m = new WebAssembly.Instance(module('\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x5f\x01\x7d\x00\x60\x01\x63\x00\x00\x0d\x03\x01\x00\x01\x07\x05\x01\x01\x74\x04\x00'));
    assert.throws(
      () => m.exports.t.type(),
      TypeError,
      "WebAssembly.Tag.prototype.type unable to produce type descriptor for the given tag"
    )
  }

  {
    /*
     * (module
     *   (tag (export "t") (param (ref func)))
     * )
     */
    let m = new WebAssembly.Instance(module('\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x06\x01\x60\x01\x64\x70\x00\x0d\x03\x01\x00\x00\x07\x05\x01\x01\x74\x04\x00'));
    assert.throws(
      () => m.exports.t.type(),
      TypeError,
      "WebAssembly.Tag.prototype.type unable to produce type descriptor for the given tag"
    )
  }
}

testArray();
testStruct();
testI31();
testRoundtrips();
testCastFailure();
testGlobal();
testTable();
testImport();
testTag();
