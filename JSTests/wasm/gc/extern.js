//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testInternalize() {
  {
    let m = instantiate(`
      (module
        (func (export "f") (param externref) (result i32)
          (i31.get_s (ref.cast (ref i31) (any.convert_extern (local.get 0)))))
      )
    `);
    assert.eq(m.exports.f(0), 0);
    assert.eq(m.exports.f(5), 5);
    assert.eq(m.exports.f(5.0), 5);
    assert.eq(m.exports.f(2 ** 30 - 1), 2 ** 30 - 1);
    assert.eq(m.exports.f(-(2 ** 30)), -(2 ** 30));
    assert.throws(
      () => m.exports.f(2 ** 30),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(2 ** 32),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(-(2 ** 30) - 1),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(5.3),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f("foo"),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(Symbol()),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
  }

  // With internalize available, it's possible to create hostref values that
  // need to be distinguished from eqref values.
  {
    let m = instantiate(`
      (module
        (func (export "f") (param externref)
          (ref.cast (ref eq) (any.convert_extern (local.get 0)))
          drop)
      )
    `);
    m.exports.f(0);
    assert.throws(
      () => m.exports.f("foo"),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(2 ** 32),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(Symbol()),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
  }

  {
    let m = instantiate(`
      (module
        (func (export "f") (param externref)
          (ref.cast (ref array) (any.convert_extern (local.get 0)))
          drop)
      )
    `);
    assert.throws(
      () => m.exports.f("foo"),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(2 ** 32),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
    assert.throws(
      () => m.exports.f(Symbol()),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
  }
}

function testExternalize() {
  {
    let m = instantiate(`
      (module
        (func (export "f") (param i32) (result externref)
          (extern.convert_any (ref.i31 (local.get 0))))
      )
    `);
    assert.eq(m.exports.f(0), 0);
    assert.eq(m.exports.f(5), 5);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (export "f") (result externref)
          (extern.convert_any (array.new 0 (i32.const 42) (i32.const 5))))
      )
    `);
    assert.eq(typeof m.exports.f(), "object");
  }
}

function testRoundtrip() {
  {
    let m = instantiate(`
      (module
        (func (param anyref) (result anyref)
          (any.convert_extern (extern.convert_any (local.get 0))))
        (func (export "f") (param i32) (result i32)
          (i31.get_s (ref.cast (ref i31) (call 0 (ref.i31 (local.get 0))))))
      )
    `);
    assert.eq(m.exports.f(0), 0);
    assert.eq(m.exports.f(5), 5);
    assert.eq(m.exports.f(2**30 - 1), 2**30 - 1);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (func (param anyref) (result anyref)
          (any.convert_extern (extern.convert_any (local.get 0))))
        (func (export "f") (result i32)
          (array.get 0
                     (ref.cast (ref 0) (call 0 (array.new 0 (i32.const 42) (i32.const 5))))
                     (i32.const 0)))
      )
    `);
    assert.eq(m.exports.f(), 42);
  }
}

function testTable() {
  {
    let m = instantiate(`
      (module
        (type (struct))
        (table (export "t") 10 externref)
        (func (table.set (i32.const 0) (extern.convert_any (struct.new 0))))
        (func (export "isStruct") (result i32)
          (ref.test (ref 0) (any.convert_extern (table.get (i32.const 1)))))
        (start 0)
      )
    `);
    assert.eq(m.exports.t.get(0) !== null, true);
    assert.eq(m.exports.t.get(1), null);
    m.exports.t.set(1, m.exports.t.get(0));
    assert.eq(m.exports.t.get(1) !== null, true);
    assert.eq(m.exports.isStruct(), 1);
  }
}

testInternalize();
testExternalize();
testRoundtrip();
testTable();
