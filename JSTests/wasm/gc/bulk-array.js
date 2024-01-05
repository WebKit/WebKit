//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testArrayFill() {
  compile(`
    (module
       (type (array (mut i32)))
       (func
         (array.fill 0 (array.new_default 0 (i32.const 10)) (i32.const 0) (i32.const 42) (i32.const 10))))
  `);

  assert.throws(
    () => compile(`
      (module
         (type (array i32))
         (func
           (array.fill 0 (array.new_default 0 (i32.const 10)) (i32.const 0) (i32.const 42) (i32.const 10))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.fill index 0 does not reference a mutable array definition, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (func
           (array.fill 0 (i32.const 3) (i32.const 0) (i32.const 42) (i32.const 10))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.fill arrayref to type I32 expected (ref null <array:0>), in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array (mut i64)))
         (func
           (array.fill 0 (array.new_default 1 (i32.const 10)) (i32.const 0) (i32.const 42) (i32.const 10))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.fill arrayref to type (ref <array:1>) expected (ref null <array:0>), in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (func
           (array.fill 0 (array.new_default 0 (i32.const 10)) (i64.const 0) (i32.const 42) (i32.const 10))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.fill offset to type I64 expected I32, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (func
           (array.fill 0 (array.new_default 0 (i32.const 10)) (i32.const 0) (i64.const 42) (i32.const 10))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.fill value to type I64 expected I32, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (func
           (array.fill 0 (array.new_default 0 (i32.const 10)) (i32.const 0) (i32.const 42) (i64.const 10))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.fill size to type I64 expected I32, in function at index 0"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (func
           (array.fill 0 (ref.null 0) (i32.const 1) (i32.const 42) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.fill to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (global (ref null 0) (ref.null 0))
         (func
           (array.fill 0 (global.get 0) (i32.const 1) (i32.const 42) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.fill to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.fill 0 (global.get 0) (i32.const 1) (i32.const 42) (i32.const 10)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.fill"
  );

  {
    const exportedFunc = instantiate(`(module (func (export "f")))`).exports.f;
    function doTest(type, defaultVal, fillVal) {
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") ${type})
           (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $arr) (i32.const 1) (global.get $fill) (i32.const 5)))
           (func (export "get") (param i32) (result ${type})
             (array.get 0 (global.get $arr) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      assert.eq(m.exports.get(0), defaultVal);
      for (let i = 1; i < 6; i++)
        assert.eq(m.exports.get(i), fillVal);
      for (let i = 6; i < 10; i++)
        assert.eq(m.exports.get(i), defaultVal);
    }
    for (const tuple of [["i32", 0, 42], ["i64", 0n, 42n], ["f32", 0, 42.0], ["f64", 0, 42.0], ["externref", null, "foo"], ["funcref", null, exportedFunc], ["i31ref", null, 42]])
      doTest(...tuple);
  }

  // Packed variant of above
  {
    function doTest(type, defaultVal, fillVal) {
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") i32)
           (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $arr) (i32.const 1) (global.get $fill) (i32.const 5)))
           (func (export "get") (param i32) (result i32)
             (array.get_s 0 (global.get $arr) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      assert.eq(m.exports.get(0), defaultVal);
      for (let i = 1; i < 6; i++)
        assert.eq(m.exports.get(i), fillVal);
      for (let i = 6; i < 10; i++)
        assert.eq(m.exports.get(i), defaultVal);
    }
    for (const tuple of [["i8", 0, 127], ["i16", 0, -27]])
      doTest(...tuple);
  }

  // Boundary conditions.
  {
    const exportedFunc = instantiate(`(module (func (export "f")))`).exports.f;
    function doTest(type, defaultVal, fillVal) {
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") ${type})
           (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $arr) (i32.const 5) (global.get $fill) (i32.const 5)))
           (func (export "get") (param i32) (result ${type})
             (array.get 0 (global.get $arr) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      for (let i = 1; i < 5; i++)
        assert.eq(m.exports.get(i), defaultVal);
      for (let i = 5; i < 10; i++)
        assert.eq(m.exports.get(i), fillVal);
    }
    for (const tuple of [["i32", 0, 42], ["i64", 0n, 42n], ["f32", 0, 42.0], ["f64", 0, 42.0], ["externref", null, "foo"], ["funcref", null, exportedFunc]])
      doTest(...tuple);
  }
}

function testArrayCopy() {
  compile(`
    (module
       (type (array (mut i32)))
       (global (ref 0) (array.new_default 0 (i32.const 10)))
       (global (ref 0) (array.new_default 0 (i32.const 10)))
       (func
         (array.copy 0 0 (global.get 0) (i32.const 0) (global.get 1) (i32.const 2) (i32.const 5))))
  `);

  assert.throws(
    () => compile(`
      (module
         (type (array i32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.copy 0 0 (global.get 0) (i32.const 0) (global.get 1) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy index 0 does not reference a mutable array definition, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array f32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (func
           (array.copy 0 1 (global.get 0) (i32.const 0) (global.get 1) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy src index 1 does not reference a subtype of dst index 0, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array i32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (func
           (array.copy 0 1 (i32.const 4) (i32.const 0) (global.get 1) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy dst to type I32 expected (ref null <array:0>), in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array i32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (func
           (array.copy 0 1 (global.get 0) (i64.const 0) (global.get 1) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy dstOffset to type I64 expected I32, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array i32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (func
           (array.copy 0 1 (global.get 0) (i32.const 0) (i32.const 4) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy src to type I32 expected (ref null <array:1>), in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array i32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (func
           (array.copy 0 1 (global.get 0) (i32.const 0) (global.get 1) (f32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy srcOffset to type F32 expected I32, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i32)))
         (type (array i32))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (func
           (array.copy 0 1 (global.get 0) (i32.const 0) (global.get 1) (i32.const 2) (f64.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.copy size to type F64 expected I32, in function at index 0"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (func
           (array.copy 0 0 (ref.null 0) (i32.const 1) (array.new_default 0 (i32.const 0)) (i32.const 42) (i32.const 1)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.copy to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (global (ref null 0) (ref.null 0))
         (func
           (array.copy 0 0 (global.get 0) (i32.const 1) (array.new_default 0 (i32.const 0)) (i32.const 42) (i32.const 1)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.copy to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (func
           (array.copy 0 0 (array.new_default 0 (i32.const 0)) (i32.const 1) (ref.null 0) (i32.const 42) (i32.const 1)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.copy to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (global (ref null 0) (ref.null 0))
         (func
           (array.copy 0 0 (array.new_default 0 (i32.const 0)) (i32.const 1) (global.get 0) (i32.const 42) (i32.const 1)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.copy to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.copy 0 0 (global.get 0) (i32.const 8) (global.get 1) (i32.const 1) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.copy"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i32)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.copy 0 0 (global.get 0) (i32.const 1) (global.get 1) (i32.const 8) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.copy"
  );

  {
    const exportedFunc = instantiate(`(module (func (export "f")))`).exports.f;
    function doTest(type, defaultVal, fillVal) {
      // D = default, F = fill value
      // src array filled as: [D, D, D, F, F, F, F, D, D, D]
      // copy positions 2-8 into offset 1 of dst
      // dst after copy:      [D, D, F, F, F, F, D, D, D, D]
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") ${type})
           (global $dst (ref 0) (array.new_default 0 (i32.const 10)))
           (global $src (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $src) (i32.const 3) (global.get $fill) (i32.const 4))
             (array.copy 0 0 (global.get $dst) (i32.const 1) (global.get $src) (i32.const 2) (i32.const 6)))
           (func (export "get") (param i32) (result ${type})
             (array.get 0 (global.get $dst) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      for (let i = 0; i < 2; i++)
        assert.eq(m.exports.get(i), defaultVal);
      for (let i = 2; i < 6; i++)
        assert.eq(m.exports.get(i), fillVal);
      for (let i = 6; i < 10; i++)
        assert.eq(m.exports.get(i), defaultVal);
    }
    for (const tuple of [["i32", 0, 42], ["i64", 0n, 42n], ["f32", 0, 42.0], ["f64", 0, 42.0], ["externref", null, "foo"], ["funcref", null, exportedFunc], ["i31ref", null, 42]])
      doTest(...tuple);
  }

  // Packed type variant of above.
  {
    function doTest(type, defaultVal, fillVal) {
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") i32)
           (global $dst (ref 0) (array.new_default 0 (i32.const 10)))
           (global $src (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $src) (i32.const 3) (global.get $fill) (i32.const 4))
             (array.copy 0 0 (global.get $dst) (i32.const 1) (global.get $src) (i32.const 2) (i32.const 6)))
           (func (export "get") (param i32) (result i32)
             (array.get_s 0 (global.get $dst) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      for (let i = 0; i < 2; i++)
        assert.eq(m.exports.get(i), defaultVal);
      for (let i = 2; i < 6; i++)
        assert.eq(m.exports.get(i), fillVal);
      for (let i = 6; i < 10; i++)
        assert.eq(m.exports.get(i), defaultVal);
    }
    for (const tuple of [["i8", 0, 127], ["i16", 0, -27]])
      doTest(...tuple);
  }

  // Ensure copy to self works.
  {
    const exportedFunc = instantiate(`(module (func (export "f")))`).exports.f;
    function doTest(type, defaultVal, fillVal) {
      // array filled as: [F, F, F, D, D, D, D, D, D, D]
      // after copy:      [F, F, F, D, D, D, D, F, F, F]
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") ${type})
           (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $arr) (i32.const 0) (global.get $fill) (i32.const 3))
             (array.copy 0 0 (global.get $arr) (i32.const 7) (global.get $arr) (i32.const 0) (i32.const 3)))
           (func (export "get") (param i32) (result ${type})
             (array.get 0 (global.get $arr) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      for (let i = 0; i < 3; i++)
        assert.eq(m.exports.get(i), fillVal);
      for (let i = 3; i < 7; i++)
        assert.eq(m.exports.get(i), defaultVal);
      for (let i = 7; i < 10; i++)
        assert.eq(m.exports.get(i), fillVal);
    }
    for (const tuple of [["i32", 0, 42], ["i64", 0n, 42n], ["f32", 0, 42.0], ["f64", 0, 42.0], ["externref", null, "foo"], ["funcref", null, exportedFunc], ["i31ref", null, 42]])
      doTest(...tuple);
  }

  // Copy to self with overlap.
  {
    const exportedFunc = instantiate(`(module (func (export "f")))`).exports.f;
    function doTest(type, defaultVal, fillVal) {
      // array filled as: [F, F, D, F, F, D, D, D, D, D]
      // after copy:      [F, F, F, F, D, F, F, D, D, D]
      const m = instantiate(`
        (module
           (type (array (mut ${type})))
           (global $fill (import "m" "g") ${type})
           (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
           (func
             (array.fill 0 (global.get $arr) (i32.const 0) (global.get $fill) (i32.const 2))
             (array.fill 0 (global.get $arr) (i32.const 3) (global.get $fill) (i32.const 2))
             (array.copy 0 0 (global.get $arr) (i32.const 2) (global.get $arr) (i32.const 0) (i32.const 5)))
           (func (export "get") (param i32) (result ${type})
             (array.get 0 (global.get $arr) (local.get 0)))
           (start 0))
      `, { m: { g: fillVal } });
      for (let i = 0; i < 4; i++)
        assert.eq(m.exports.get(i), fillVal);
      assert.eq(m.exports.get(4), defaultVal);
      for (let i = 5; i < 7; i++)
        assert.eq(m.exports.get(i), fillVal);
      for (let i = 7; i < 10; i++)
        assert.eq(m.exports.get(i), defaultVal);
    }
    for (const tuple of [["i32", 0, 42], ["i64", 0n, 42n], ["f32", 0, 42.0], ["f64", 0, 42.0], ["externref", null, "foo"], ["funcref", null, exportedFunc], ["i31ref", null, 42]])
      doTest(...tuple);
  }

  // Copy to self with overlap using various reftype values.
  {
    const exportedFunc = instantiate(`(module (func (export "f")))`).exports.f;
    function doTest(v1, v2, v3) {
      // array filled as: [V1, V2, V3]
      // after copy:      [V1, V1, V2]
      const m = instantiate(`
        (module
           (type (array (mut externref)))
           (global $v1 (import "m" "g1") externref)
           (global $v2 (import "m" "g2") externref)
           (global $v3 (import "m" "g3") externref)
           (global $arr (ref 0) (array.new_fixed 0 3 (global.get $v1) (global.get $v2) (global.get $v3)))
           (func
             (array.copy 0 0 (global.get $arr) (i32.const 1) (global.get $arr) (i32.const 0) (i32.const 2)))
           (func (export "get") (param i32) (result externref)
             (array.get 0 (global.get $arr) (local.get 0)))
           (start 0))
      `, { m: { g1: v1, g2: v2, g3: v3 } });
      assert.eq(m.exports.get(0), v1);
      assert.eq(m.exports.get(1), v1);
      assert.eq(m.exports.get(2), v2);
    }
    doTest({ x: "foo" }, { y: "bar" }, { z: "baz" });
    doTest("foo", 53n, 18);
    doTest(Symbol(), Symbol(), Symbol());
  }
}

function testArrayInitElem() {
  compile(`
    (module
       (type (array (mut funcref)))
       (global (ref 0) (array.new_default 0 (i32.const 10)))
       (elem funcref (item (ref.func 0)) (item (ref.func 0)))
       (func)
       (func
         (array.init_elem 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5))))
  `);

  assert.throws(
    () => compile(`
      (module
         (type (array funcref))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_elem index 0 does not reference a mutable array definition, in function at index 1"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 1 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: element index 1 is invalid, limit is 1, in function at index 1"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (i32.const 42) (i32.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_elem dst to type I32 expected (ref null <array:0>), in function at index 1"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i64.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_elem dstOffset to type I64 expected I32, in function at index 1"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 0) (f32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_elem srcOffset to type F32 expected I32, in function at index 1"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 0) (i32.const 2) (f64.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_elem size to type F64 expected I32, in function at index 1"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut funcref)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (ref.null 0) (i32.const 0) (i32.const 2) (i32.const 5)))
         (start 1))
    `),
    WebAssembly.RuntimeError,
    "array.init_elem to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut funcref)))
         (global (ref null 0) (ref.null 0))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5)))
         (start 1))
    `),
    WebAssembly.RuntimeError,
    "array.init_elem to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 15) (i32.const 2) (i32.const 5)))
         (start 1))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_elem"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func)
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 2) (i32.const 10) (i32.const 5)))
         (start 1))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_elem"
  );

  {
    const m = instantiate(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func (export "f"))
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 8) (i32.const 0) (i32.const 2)))
         (func (export "get") (param i32) (result funcref)
           (array.get 0 (global.get 0) (local.get 0)))
         (start 1))
    `);
    for (let i = 0; i < 8; i++)
      assert.eq(m.exports.get(i), null);
    for (let i = 8; i < 10; i++)
      assert.eq(m.exports.get(i), m.exports.f);
  }

  {
    const m = instantiate(`
      (module
         (type (array (mut funcref)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (elem funcref (item (ref.func 0)) (item (ref.func 0)))
         (func (export "f"))
         (func
           (array.init_elem 0 0 (global.get 0) (i32.const 0) (i32.const 1) (i32.const 1)))
         (func (export "get") (param i32) (result funcref)
           (array.get 0 (global.get 0) (local.get 0)))
         (start 1))
    `);
    assert.eq(m.exports.get(0), m.exports.f);
    for (let i = 1; i < 10; i++)
      assert.eq(m.exports.get(i), null);
  }

  {
    const m = instantiate(`
      (module
         (type (struct))
         (type (array (mut (ref null 0))))
         (global (ref 1) (array.new_default 1 (i32.const 10)))
         (elem (ref 0) (item (struct.new 0)) (item (struct.new 0)))
         (func
           (array.init_elem 1 0 (global.get 0) (i32.const 8) (i32.const 0) (i32.const 2)))
         (func (export "get") (param i32) (result i32)
           (ref.test (ref 0) (array.get 1 (global.get 0) (local.get 0))))
         (start 0))
    `);
    for (let i = 0; i < 8; i++)
      assert.eq(m.exports.get(i), 0);
    for (let i = 8; i < 10; i++)
      assert.eq(m.exports.get(i), 1);
  }
}

function testArrayInitData() {
  compile(`
    (module
       (type (array (mut i8)))
       (global (ref 0) (array.new_default 0 (i32.const 10)))
       (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
       (func
         (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5))))
  `);

  assert.throws(
    () => compile(`
      (module
         (type (array i8))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_data index 0 does not reference a mutable array definition, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i8)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 1 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: data segment index 1 is invalid, limit is 1, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i8)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (i32.const 42) (i32.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_data dst to type I32 expected (ref null <array:0>), in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i8)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i64.const 0) (i32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_data dstOffset to type I64 expected I32, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i8)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (f32.const 2) (i32.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_data srcOffset to type F32 expected I32, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
         (type (array (mut i8)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 2) (f64.const 5))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: array.init_data size to type F64 expected I32, in function at index 0"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i8)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (ref.null 0) (i32.const 0) (i32.const 2) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.init_data to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i8)))
         (global (ref null 0) (ref.null 0))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.init_data to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i8)))
         (global (ref null 0) (ref.null 0))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 2) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "array.init_data to a null reference"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i8)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 1) (i32.const 0) (i32.const 10)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_data"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i16)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 6) (i32.const 0) (i32.const 5)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_data"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut i16)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 0) (i32.const 6)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_data"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut f64)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 9) (i32.const 0) (i32.const 2)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_data"
  );

  assert.throws(
    () => instantiate(`
      (module
         (type (array (mut f64)))
         (global (ref 0) (array.new_default 0 (i32.const 10)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (func
           (array.init_data 0 0 (global.get 0) (i32.const 0) (i32.const 0) (i32.const 2)))
         (start 0))
    `),
    WebAssembly.RuntimeError,
    "Out of bounds array.init_data"
  );

  {
     const m = instantiate(`
       (module
          (type (array (mut i8)))
          (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
          (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
          (func
            (array.init_data 0 0 (global.get $arr) (i32.const 0) (i32.const 0) (i32.const 10)))
          (func (export "get") (param i32) (result i32)
            (array.get_s 0 (global.get $arr) (local.get 0)))
          (start 0))
     `);
     for (let i = 0; i < 10; i++)
       assert.eq(m.exports.get(i), i);
  }

  {
    const m = instantiate(`
      (module
         (type (array (mut i16)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")
         (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.init_data 0 0 (global.get $arr) (i32.const 0) (i32.const 0) (i32.const 5)))
         (func (export "get") (param i32) (result i32)
           (array.get_s 0 (global.get $arr) (local.get 0)))
         (start 0))
    `);
    assert.eq(m.exports.get(0), 0x0100);
    assert.eq(m.exports.get(1), 0x0302);
    assert.eq(m.exports.get(2), 0x0504);
    assert.eq(m.exports.get(3), 0x0706);
    assert.eq(m.exports.get(4), 0x0908);
    for (let i = 5; i < 10; i++)
      assert.eq(m.exports.get(i), 0)
  }

  {
    const m = instantiate(`
      (module
         (type (array (mut i32)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07")
         (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.init_data 0 0 (global.get $arr) (i32.const 1) (i32.const 0) (i32.const 2)))
         (func (export "get") (param i32) (result i32)
           (array.get 0 (global.get $arr) (local.get 0)))
         (start 0))
    `);
    assert.eq(m.exports.get(0), 0);
    assert.eq(m.exports.get(1), 0x03020100);
    assert.eq(m.exports.get(2), 0x07060504);
    for (let i = 3; i < 10; i++)
      assert.eq(m.exports.get(i), 0)
  }

  {
    const m = instantiate(`
      (module
         (type (array (mut i64)))
         (data "\\00\\01\\02\\03\\04\\05\\06\\07")
         (global $arr (ref 0) (array.new_default 0 (i32.const 10)))
         (func
           (array.init_data 0 0 (global.get $arr) (i32.const 1) (i32.const 0) (i32.const 1)))
         (func (export "get") (param i32) (result i64)
           (array.get 0 (global.get $arr) (local.get 0)))
         (start 0))
    `);
    assert.eq(m.exports.get(0), 0n);
    assert.eq(m.exports.get(1), 0x0706050403020100n);
    for (let i = 2; i < 10; i++)
      assert.eq(m.exports.get(i), 0n)
  }
}

testArrayFill();
testArrayCopy();
testArrayInitElem();
testArrayInitData();
