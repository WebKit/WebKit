//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true")

import * as assert from "../assert.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

async function testTableInitParsing() {
  /*
   * (module
   *   (table 10 externref (ref.null extern)))
   */
   module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x84\x80\x80\x80\x00\x01\x6f\x00\x0a");

  /*
   * (module
   *   (table 10 externref (ref.null func)))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x89\x80\x80\x80\x00\x01\x40\x00\x6f\x00\x0a\xd0\x70\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 23: Table init_expr opcode of type RefNull doesn't match table's type RefNull"
  )

  /*
   * (module
   *   (table 10 externref (ref.null func)))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x89\x80\x80\x80\x00\x01\x40\x00\x6f\x00\x0a\xd0\x70\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 23: Table init_expr opcode of type RefNull doesn't match table's type RefNull"
  )

  // Invalid encoding for imported table
  /*
   * (module
   *   (table (import "m" "g") 10 externref (ref.null extern)))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x89\x80\x80\x80\x00\x01\x01\x6d\x01\x79\x01\x40\x00\x6f\x00\x0a\xd0\x70\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 21: can't parse Table type"
  )

  // Marker byte for initialized table must be 0x40
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x89\x80\x80\x80\x00\x01\x41\x00\x6f\x00\x0a\xd0\x70\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 16: can't parse Table type"
  )

  // Reserved byte should be zero
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x89\x80\x80\x80\x00\x01\x40\x01\x6f\x00\x0a\xd0\x70\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 17: can't parse explicitly initialized Table's reserved byte"
  )

  // Missing table type
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x83\x80\x80\x80\x00\x01\x40\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 17: can't parse Table type"
  )

  // Truncated table type
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x84\x80\x80\x80\x00\x01\x40\x00\x6f"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 18: can't parse resizable limits flags"
  )

  // Missing init expr
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x86\x80\x80\x80\x00\x01\x40\x00\x6f\x00\x0a"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: can't get init_expr's opcode"
  )

  // Init expr immediately ends with end marker
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x87\x80\x80\x80\x00\x01\x40\x00\x6f\x00\x0a\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 21: unknown init_expr opcode 11"
  )
}

async function testTableInitRuntime() {
  /*
   * (module
   *   (global (import "m" "g") externref)
   *   (table (export "t") 10 externref (global.get 0)))
   */
  {
    const m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x02\x88\x80\x80\x80\x00\x01\x01\x6d\x01\x67\x03\x6f\x00\x04\x89\x80\x80\x80\x00\x01\x40\x00\x6f\x00\x0a\x23\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00"),
      { m: { g: "hello" } }
    );
    for (var i = 0; i < m.exports.t.length; i++) {
      assert.eq(m.exports.t.get(i), "hello");
    }
  }

  /*
   * (module
   *   (func (result i32) (i32.const 42))
   *   (table (export "t") 10 funcref (ref.func 0)))
   */
  {
    const m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x85\x80\x80\x80\x00\x01\x60\x00\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x04\x89\x80\x80\x80\x00\x01\x40\x00\x70\x00\x0a\xd2\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x0a\x8a\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\x41\x2a\x0b")
    );
    for (var i = 0; i < m.exports.t.length; i++) {
      assert.isFunction(m.exports.t.get(i));
      assert.eq(m.exports.t.get(i)(), 42);
    }
  }

  /*
   * (module
   *   (func (result i32) (i32.const 42))
   *   (table (export "t") 10 (ref 0) (ref.func 0)))
   */
  {
    const m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x85\x80\x80\x80\x00\x01\x60\x00\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x04\x8a\x80\x80\x80\x00\x01\x40\x00\x63\x00\x00\x0a\xd2\x00\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x0a\x8a\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\x41\x2a\x0b")
    );
    for (var i = 0; i < m.exports.t.length; i++) {
      assert.isFunction(m.exports.t.get(i));
      assert.eq(m.exports.t.get(i)(), 42);
    }
  }

  /*
   * (module
   *   (table (export "t") 10 externref (ref.null extern)))
   */
  {
    const m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x84\x80\x80\x80\x00\x01\x6f\x00\x0a\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00")
    );
    for (var i = 0; i < m.exports.t.length; i++) {
      assert.eq(m.exports.t.get(i), null);
    }
  }
}

assert.asyncTest(testTableInitParsing());
assert.asyncTest(testTableInitRuntime());
