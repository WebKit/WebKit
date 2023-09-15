//@ runWebAssemblySuite("--useWebAssemblyExtendedConstantExpressions=true")
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

// Test non-extended const expressions under the feature flag to ensure they keep working.
async function testConstExprFastPaths() {
  /*
   * (module
   *   (global (export "g") i32 (i32.const 42)))
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x86\x80\x80\x80\x00\x01\x7f\x00\x41\x2a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 42);
  }

  /*
   * (module
   *   (global (export "g") i64 (i64.const 42)))
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x86\x80\x80\x80\x00\x01\x7e\x00\x42\x2a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 42n);
  }

  /*
   * (module
   *   (global (export "g") f32 (f32.const 32)))
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7d\x00\x43\x00\x00\x00\x42\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 32);
  }

  /*
   * (module
   *   (global (export "g") f64 (f64.const 42)))
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8d\x80\x80\x80\x00\x01\x7c\x00\x44\x00\x00\x00\x00\x00\x00\x45\x40\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 42);
  }

  /*
   * (module
   *   (global (import "m" "gi1") i32)
   *   (global (export "g") i32 (global.get 0))
   *   )
   */
  {
    let m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x8a\x80\x80\x80\x00\x01\x01\x6d\x03\x67\x69\x31\x03\x7f\x00\x06\x86\x80\x80\x80\x00\x01\x7f\x00\x23\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x01"),
      { m: { gi1: 16 }}
    );
    assert.eq(m.exports.g.value, 16);
  }

  /*
   * (module
   *   (func)
   *   (global (export "g") funcref (ref.func 0)))
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x06\x86\x80\x80\x80\x00\x01\x70\x00\xd2\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00\x0a\x88\x80\x80\x80\x00\x01\x82\x80\x80\x80\x00\x00\x0b"));
    assert.isFunction(m.exports.g.value);
  }

  /*
   * (module
   *   (global (export "g") externref (ref.null extern)))
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x86\x80\x80\x80\x00\x01\x6f\x00\xd0\x6f\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, null);
  }
}

async function testExtendedConstGlobal() {
  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 1) (i32.const 42)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x41\x2a\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 43);
  }

  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 2147483647) (i32.const 1)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8d\x80\x80\x80\x00\x01\x7f\x00\x41\xff\xff\xff\xff\x07\x41\x01\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, -2147483648);
  }

  /*
   * (module
   *   (global (export "g") i32 (i32.sub (i32.const -2147483648) (i32.const 1)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8d\x80\x80\x80\x00\x01\x7f\x00\x41\x80\x80\x80\x80\x78\x41\x01\x6b\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 2147483647);
  }

  /*
   * (module
   *   (global (export "g") i64 (i64.add (i64.const 1) (i64.const 42)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7e\x00\x42\x01\x42\x2a\x7c\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 43n);
  }

  /*
   * (module
   *   (global (export "g") i64 (i64.add (i64.const 9223372036854775807) (i64.const 1)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x92\x80\x80\x80\x00\x01\x7e\x00\x42\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x42\x01\x7c\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, -9223372036854775808n);
  }

  /*
   * (module
   *   (global (export "g") i32 (i32.add (i64.const 1) (i32.const 42)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x42\x01\x41\x2a\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: I32Add left value type mismatch"
  );


  /*
   * (module
   *   (global (export "g") i64 (i64.sub (i64.const -9223372036854775808) (i64.const 1)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x92\x80\x80\x80\x00\x01\x7e\x00\x42\x80\x80\x80\x80\x80\x80\x80\x80\x80\x7f\x42\x01\x7d\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 9223372036854775807n);
  }

  /*
   * (module
   *   (global (export "g") i32 (i32.mul (i32.const 2) (i32.const 42)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x41\x02\x41\x2a\x6c\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 84);
  }

  /*
   * (module
   *   (global (export "g") i32 (i32.mul (i32.const 2147483647) (i32.const 2)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8d\x80\x80\x80\x00\x01\x7f\x00\x41\xff\xff\xff\xff\x07\x41\x02\x6c\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, -2);
  }

  /*
   * (module
   *   (global (export "g") i64 (i64.mul (i64.const 2) (i64.const 42)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7e\x00\x42\x02\x42\x2a\x7e\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 84n);
  }

  /*
   * (module
   *   (global (export "g") i64 (i64.mul (i64.const 2147483647) (i32.const 2)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8d\x80\x80\x80\x00\x01\x7f\x00\x41\xff\xff\xff\xff\x07\x41\x02\x6c\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, -2);
  }

  /*
   * (module
   *   (global (export "g") i64 (i64.mul (i64.const 9223372036854775807) (i64.const 2)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x92\x80\x80\x80\x00\x01\x7e\x00\x42\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x42\x02\x7e\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, -2n);
  }

  /*
   * (module
   *   (global (export "g") i32 (i32.div_s (i32.const 42) (i32.const 6)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x41\x2a\x41\x06\x6d\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 22: Invalid instruction for constant expression"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 42) (v128.const i32x4 1 2 3 4)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x99\x80\x80\x80\x00\x01\x7f\x00\x41\x2a\xfd\x0c\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: I32Add right value type mismatch"
  );

  /*
   * (module
   *   (global (export "g") v128 (i32.add (i32.const 42) (i32.const 6)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7b\x00\x41\x2a\x41\x06\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. I32 is not a V128"
  );

  /*
   * (module
   *   (global (import "m" "gi1") i32)
   *   (global (export "g") i32 (i32.add (global.get 0) (i32.const 42)))
   *   )
   */
  {
    let m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x8a\x80\x80\x80\x00\x01\x01\x6d\x03\x67\x69\x31\x03\x7f\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x23\x00\x41\x2a\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x01"),
      { m: { gi1: 16 }}
    );
    assert.eq(m.exports.g.value, 58);
  }

  /*
   * (module
   *   (global (export "g") i64
   *     (i64.add
   *       (i64.mul (i64.add (i64.const 1) (i64.const 3))
   *                (i64.const 4))
   *       (i64.sub (i64.const 16) (i64.const 5))))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x92\x80\x80\x80\x00\x01\x7e\x00\x42\x01\x42\x03\x7c\x42\x04\x7e\x42\x10\x42\x05\x7d\x7c\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"));
    assert.eq(m.exports.g.value, 27n);
  }
}

async function testExtendedConstElement() {
  // Test element segment kind 0.
  /*
   * (module
   *   (table (export "t") 64 funcref)
   *   (func)
   *   (elem (table 0) (offset (i32.add (i32.const 1) (i32.const 42))) funcref (ref.func 0))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x40\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x09\x8a\x80\x80\x80\x00\x01\x00\x41\x01\x41\x2a\x6a\x0b\x01\x00\x0a\x88\x80\x80\x80\x00\x01\x82\x80\x80\x80\x00\x00\x0b"));
    assert.eq(m.exports.t.get(0), null);
    assert.eq(m.exports.t.get(42), null);
    assert.isFunction(m.exports.t.get(43));
    assert.eq(m.exports.t.get(44), null);
  }

  // Test element segment kind 4
  /*
   * (module
   *   (table (export "t") 64 funcref)
   *   (elem (table 0) (offset (i32.add (i32.const 1) (i32.const 42))) funcref (ref.null func))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x40\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x09\x8c\x80\x80\x80\x00\x01\x04\x41\x01\x41\x2a\x6a\x0b\x01\xd0\x70\x0b"));
    assert.eq(m.exports.t.get(43), null);
  }

  // Test element segment kind 6.
  /*
   * (module
   *   (global (import "m" "gi1") externref)
   *   (table (export "t") 64 externref)
   *   (elem (table 0) (offset (i32.add (i32.const 1) (i32.const 42))) externref (ref.null extern))
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x84\x80\x80\x80\x00\x01\x6f\x00\x40\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x09\x8e\x80\x80\x80\x00\x01\x06\x00\x41\x01\x41\x2a\x6a\x0b\x6f\x01\xd0\x6f\x0b"));
    assert.eq(m.exports.t.get(43), null);
  }

  // FIXME: this requires changing how element segment initialization vectors are parsed.
  // Test element segment kind 6.with element init expression.
  /*
   * (module
   *   (global (import "m" "gi1") externref)
   *   (table (export "t") 64 externref)
   *   (elem (table 0) (offset (i32.add (i32.const 1) (i32.const 42))) externref (global.get 0))
   *   )
   */
  //{
  //  let obj = "hello";
  //  let m = new WebAssembly.Instance(
  //    module("\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x8a\x80\x80\x80\x00\x01\x01\x6d\x03\x67\x69\x31\x03\x6f\x00\x04\x84\x80\x80\x80\x00\x01\x6f\x00\x40\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x09\x8e\x80\x80\x80\x00\x01\x06\x00\x41\x01\x41\x2a\x6a\x0b\x6f\x01\x23\x00\x0b"),
  //    { m: { gi1: obj } }
  //  );
  //  assert.eq(m.exports.t.get(0), null);
  //  assert.eq(m.exports.t.get(42), null);
  //  assert.eq(m.exports.t.get(43), obj);
  //  assert.eq(m.exports.t.get(44), null);
  //}
}

async function testExtendedConstData() {
  // Test data segment kind 0.
  /*
   * (module
   *   (memory (export "m") 40)
   *   (data (memory 0) (offset (i32.add (i32.const 1) (i32.const 15))) "hello")
   *   )
   */
  {
    let m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x05\x83\x80\x80\x80\x00\x01\x00\x28\x07\x85\x80\x80\x80\x00\x01\x01\x6d\x02\x00\x0b\x8e\x80\x80\x80\x00\x01\x00\x41\x01\x41\x0f\x6a\x0b\x05\x68\x65\x6c\x6c\x6f"));
    let arr = new Uint8Array(m.exports.m.buffer);
    assert.eq(arr[0], 0);
    assert.eq(arr[15], 0);
    assert.eq(arr[16], 0x68);
    assert.eq(arr[21], 0);
  }
}

async function testInvalidConstExprs() {
  /*
   * (module
   *   (global (export "g") i32 (block (result i32) (i32.const 1)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x02\x7f\x41\x01\x0b\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 18: unknown init_expr opcode 2"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.const 42) (block (result i32) (i32.const 1)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8b\x80\x80\x80\x00\x01\x7f\x00\x41\x2a\x02\x7f\x41\x01\x0b\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 21: Invalid instruction for constant expression"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 1) (i32.const 2)) end drop)
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8b\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x41\x02\x6a\x0b\x1a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 14: parsing ended before the end of Global section"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 1) (i32.const 2)) (i32.add (i32.const 1) (i32.const 2)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8e\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x41\x02\x6a\x41\x01\x41\x02\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate:  block with type: () -> [I32] returns: 1 but stack has: 2 values"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 1) (i32.const 2)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x8e\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x41\x02\x6a\x41\x01\x41\x02\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate:  block with type: () -> [I32] returns: 1 but stack has: 2 values"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.const 1) (br 0))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x88\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x0c\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 21: Invalid instruction for constant expression"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.const 1) unreachable (i32.const 1))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x00\x41\x01\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: Invalid instruction for constant expression"
  );

  /*
   * (module
   *   (global (export "g") i32 (local.tee 0 (i32.const 1)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x88\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x22\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "attempt to use unknown local 0, the number of locals is 0"
  );

  /*
   * (module
   *   (global (export "g") i32 (i32.const 1) i32.add)
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x87\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 3: can't pop empty stack in binary left"
  );

  // This raises an error in terms of table.get validation instead of invalid instruction.
  // The error message could be better, but it would raise invalid instruction if the table.get were validly encoded.
  /*
   * (module
   *   (global (export "g") i32 (i32.const 1) table.get)
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x88\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x25\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: table index 0 is invalid, limit is 0"
  );
}

assert.asyncTest(testConstExprFastPaths());
assert.asyncTest(testExtendedConstGlobal());
assert.asyncTest(testExtendedConstElement());
assert.asyncTest(testExtendedConstData());
assert.asyncTest(testInvalidConstExprs());
