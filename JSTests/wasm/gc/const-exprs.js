//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--useWebAssemblyExtendedConstantExpressions=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

async function testGCConstExprs() {
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (global (export "g") (ref 0) (array.new 0 (i32.const 42) (i32.const 10)))
        (func (export "f") (param i32) (result i32)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(3), 42);
  }

  {
    let m = instantiate(`
      (module
        (type (struct (field i32) (field i64)))
        (global (export "g") (ref 0) (struct.new 0 (i32.const 42) (i64.const 84)))
        (func (export "f1") (result i32)
          (struct.get 0 0 (global.get 0)))
        (func (export "f2") (result i64)
          (struct.get 0 1 (global.get 0)))
      )
    `);
    assert.eq(m.exports.f1(), 42);
    assert.eq(m.exports.f2(), 84n);
  }

  {
    let m = instantiate(`
      (module
        (type (array externref))
        (global (export "g") (ref 0) (array.new_default 0 (i32.const 10)))
        (func (export "f") (param i32) (result externref)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(3), null);
  }

  {
    let m = instantiate(`
      (module
        (type (struct (field i32) (field i64)))
        (global (export "g") (ref 0) (struct.new_default 0))
        (func (export "f1") (result i32)
          (struct.get 0 0 (global.get 0)))
        (func (export "f2") (result i64)
          (struct.get 0 1 (global.get 0)))
      )
    `);
    assert.eq(m.exports.f1(), 0);
    assert.eq(m.exports.f2(), 0n);
  }

  {
    let m = instantiate(`
      (module
        (type (array i32))
        (global (export "g") (ref 0)
          (array.new_fixed 0 4 (i32.const 0) (i32.const 1) (i32.const 2) (i32.const 3)))
        (func (export "f") (param i32) (result i32)
          (array.get 0 (global.get 0) (local.get 0)))
      )
    `);
    assert.eq(m.exports.f(0), 0);
    assert.eq(m.exports.f(1), 1);
    assert.eq(m.exports.f(2), 2);
    assert.eq(m.exports.f(3), 3);
  }

  // Test array.new with arithmetic and global.get operations
  {
    let m = instantiate(`
      (module
        (type (array i32))
        (global (import "m" "gi1") i32)
        (global (import "m" "gi2") i32)
        (global (export "g") (ref 0)
          (array.new 0 (i32.add (global.get 0) (i32.const 3)) (global.get 1)))
        (func (export "f") (param i32) (result i32)
          (array.get 0 (global.get 2) (local.get 0)))
      )`,
      { m: { gi1: 42, gi2: 10} }
    );
    assert.eq(m.exports.f(3), 45);
  }

  // Test struct.new with arithmetic & global.get
  {
    let m = instantiate(`
      (module
        (type (struct (field i32)))
        (global (import "m" "gi1") i32)
        (global (import "m" "gi2") i32)
        (global (export "g") (ref 0)
          (struct.new 0 (i32.add (global.get 0) (global.get 1))))
        (func (export "f") (result i32)
          (struct.get 0 0 (global.get 2)))
      )`,
      { m: { gi1: 42, gi2: 10} }
    );
    assert.eq(m.exports.f(), 52);
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (ref i31) (ref.i31 (i32.const 555))))
    `);
    assert.eq(m.exports.g.value, 555);
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (ref i31) (ref.i31 (i32.const 0x4000_0000))))
    `);
    assert.eq(m.exports.g.value, -0x40000000);
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (ref i31) (ref.i31 (i32.const 0x4000_0000))))
    `);
    assert.eq(m.exports.g.value, -0x40000000);
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (ref i31) (ref.i31 (i32.const 0xaaaa_aaaa))))
    `);
    assert.eq(m.exports.g.value, 0x2aaaaaaa);
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") (ref i31) (ref.i31 (i32.const 0x7fff_ffff))))
    `);
    assert.eq(m.exports.g.value, -1);
  }

  {
    let m = instantiate(`
      (module
        (global (export "g") externref (extern.convert_any (ref.i31 (i32.const 555)))))
    `);
    assert.eq(m.exports.g.value, 555);
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (global (export "g") externref (extern.convert_any (struct.new 0))))
    `);
    assert.isObject(m.exports.g.value);
  }

  {
    let m = compile(`
        (module
          (global (import "m" "gi1") externref)
          (global (export "g") (ref null any) (any.convert_extern (global.get 0)))
          (func (export "f") (result i32)
            (i31.get_s (ref.cast (ref i31) (global.get 1))))
        )
    `);
    let inst1 = new WebAssembly.Instance(m, { m: { gi1: 42 } });
    let inst2 = new WebAssembly.Instance(m, { m: { gi1: 2 ** 31 - 1 } });
    assert.eq(inst1.exports.g.value, 42);
    assert.eq(inst1.exports.f(), 42);

    assert.eq(inst2.exports.g.value, 2 ** 31 - 1);
    assert.throws(
      () => inst2.exports.f(),
      WebAssembly.RuntimeError,
      "ref.cast failed to cast reference to target heap type"
    );
  }
}

async function testInvalidConstExprs() {
  assert.throws(
    () => compile(`
      (module
        (type (struct (field i32)))
        (global (export "g") (ref 0) (struct.new 0 (i64.const 1))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: argument type mismatch in struct.new, got I64, expected I32"
  );

  assert.throws(
    () => compile(`
      (module
        (type (struct (field i32)))
        (global (export "g") i32 (struct.new 0 (i32.const 1))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (ref <struct:0>) is not a I32"
  );

  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (global (export "g") (ref 0) (array.len (array.new 0 (i32.const 1) (i32.const 1)))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 37: Invalid instruction for constant expression"
  );
}

async function testConstExprGlobalOrdering() {
  compile(`
    (module
      (global i32 (i32.const 0))
      (global i32 (global.get 0)))
  `);

  compile(`
    (module
      (global i32 (i32.const 0))
      (global i32 (i32.const 1))
      (global i32 (i32.const 2))
      (global i32 (global.get 1))
      (global i32 (global.get 3)))
  `);

  {
    let m = instantiate(`
      (module
        (global i32 (i32.add (i32.const 0) (i32.const 1)))
        (global (export "g") i32 (i32.add (i32.const 1 (global.get 0)))))
    `);

    assert.eq(m.exports.g.value, 2);
  }

  compile(`
    (module
      (global (import "m" "g") externref)
      (table 10 externref (global.get 0))
      )
  `);

  compile(`
    (module
      (global i32 (i32.const 0))
      (global i32 (i32.const 1))
      (global i32 (i32.const 2))
      (global i32 (global.get 1))
      (global i32 (global.get 3)))
  `);

  compile(`
    (module
      (table (export "t") 64 funcref)
      (global i32 (i32.const 5))
      (elem (table 0) (offset (i32.add (global.get 0) (i32.const 42))) funcref (ref.null func)))
  `);

  assert.throws(
    () => compile(`
      (module
        (global i32 (global.get 1))
        (global i32 (i32.const 0)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 19: get_global's index 1 exceeds the number of globals 0"
  );

  assert.throws(
    () => compile(`
      (module
        (table 10 externref (global.get 0))
        (global externref (ref.null extern)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 22: get_global's index 0 exceeds the number of globals 0"
  );
}

assert.asyncTest(testGCConstExprs());
assert.asyncTest(testInvalidConstExprs());
assert.asyncTest(testConstExprGlobalOrdering())
