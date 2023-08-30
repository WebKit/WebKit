//@ runWebAssemblySuite("--useWebAssemblyExtendedConstantExpressions=false")
import * as assert from "../assert.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

// Test to make sure error paths work when feature flag is off.
async function testConstExprErrorPaths() {
  /*
   * (module
   *   (global (export "g") i32 (i32.add (i32.const 1) (i32.const 42)))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x06\x89\x80\x80\x80\x00\x01\x7f\x00\x41\x01\x41\x2a\x6a\x0b\x07\x85\x80\x80\x80\x00\x01\x01\x67\x03\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 19: init_expr should end with end, ended with 65"
  );

  /*
   * (module
   *   (table (export "t") 64 funcref)
   *   (func)
   *   (elem (table 0) (offset (i32.add (i32.const 1) (i32.const 42))) funcref (ref.func 0))
   *   )
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x40\x07\x85\x80\x80\x80\x00\x01\x01\x74\x01\x00\x09\x8a\x80\x80\x80\x00\x01\x00\x41\x01\x41\x2a\x6a\x0b\x01\x00\x0a\x88\x80\x80\x80\x00\x01\x82\x80\x80\x80\x00\x00\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 57: init_expr should end with end, ended with 65"
  )
}

assert.asyncTest(testConstExprErrorPaths());
