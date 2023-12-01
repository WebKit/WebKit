//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true")
import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

async function br_on_null() {
  /*
  (module
    (func (export "f") (param funcref) (result i32)
      (br_on_null 0 (i32.const 1) (local.get 0))
      drop drop
      (i32.const 0))
  )
  */
  {
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x70\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x41\x01\x20\x00\xd5\x00\x1a\x1a\x41\x00\x0b"));
    assert.eq(instance.exports.f(null), 1);
    assert.eq(instance.exports.f(instance.exports.f), 0);
  }

  /*
  (module
    (func (import "m" "f") (param (ref extern)) (result i32))
    (func (export "g") (param externref) (result i32)
      (br_on_null 0 (i32.const 1) (local.get 0))
      (br 0 (call 0)))
  )
  */
  {
    let instance = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x01\x64\x6f\x01\x7f\x60\x01\x6f\x01\x7f\x02\x87\x80\x80\x80\x00\x01\x01\x6d\x01\x66\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x07\x85\x80\x80\x80\x00\x01\x01\x67\x00\x01\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x00\x41\x01\x20\x00\xd5\x00\x10\x00\x0c\x00\x0b"),
      { m: { f: (extern) => 0 } }
    );
    assert.eq(instance.exports.g(null), 1);
    assert.eq(instance.exports.g(instance.exports.f), 0);
  }

  // Ensure branch target matches.
  /*
  (module
    (func (export "f") (param funcref) (result i32)
      (br_on_null 0 (local.get 0))
      drop)
  )
  */
  assert.throws(
    () => new WebAssembly.Module(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x70\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x20\x00\xd5\x00\x1a\x0b")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: branch out of function on expression stack of size 0, but block, (RefNull) -> [I32] expects 1 values, in function at index 0"
  )

  /*
  (module
    (func (param) (result)
      (br_on_null 0)
      drop)
  )
  */
  assert.throws(
    () => new WebAssembly.Module(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8b\x80\x80\x80\x00\x01\x85\x80\x80\x80\x00\x00\xd5\x00\x1a\x0b")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 3: can't pop empty stack in br_on_null, in function at index 0"
  )

  /*
  (module
    (func (param) (result)
      (br_on_null 0 (i32.const 1))
      drop)
  )
  */
  assert.throws(
    () => new WebAssembly.Module(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x41\x01\xd5\x00\x1a\x0b")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: br_on_null ref to type I32 expected a reference type, in function at index 0"
  )
}

async function br_on_non_null() {
  /*
  (module
    (func (export "f") (param funcref) (result i32 (ref null func))
      (br_on_non_null 0 (i32.const 1) (local.get 0))
      drop
      (i32.const 0) (ref.null func))
  )
  */
  {
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x87\x80\x80\x80\x00\x01\x60\x01\x70\x02\x7f\x70\x03\x82\x80\x80\x80\x00\x01\x00\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0a\x93\x80\x80\x80\x00\x01\x8d\x80\x80\x80\x00\x00\x41\x01\x20\x00\xd6\x00\x1a\x41\x00\xd0\x70\x0b"));
    assert.eq(instance.exports.f(null)[0], 0);
    assert.eq(instance.exports.f(instance.exports.f)[0], 1);
  }

  // Ensure branch target matches.
  /*
  (module
    (func (export "f") (param funcref) (result i32)
      (br_on_non_null 0 (local.get 0))
      drop)
  )
  */
  assert.throws(
    () => new WebAssembly.Module(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x70\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x20\x00\xd6\x00\x1a\x0b")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: branch's stack type is not a subtype of block's type branch target type. Stack value has type (ref func) but branch target expects a value of I32 at index 0, in function at index 0"
  )

  /*
  (module
    (func (param) (result)
      (br_on_non_null 0)
      drop)
  )
  */
  assert.throws(
    () => new WebAssembly.Module(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8b\x80\x80\x80\x00\x01\x85\x80\x80\x80\x00\x00\xd6\x00\x1a\x0b")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 3: can't pop empty stack in br_on_non_null, in function at index 0"
  )

  /*
  (module
    (func (param) (result)
      (br_on_non_null 0 (i32.const 1))
      drop)
  )
  */
  assert.throws(
    () => new WebAssembly.Module(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8d\x80\x80\x80\x00\x01\x87\x80\x80\x80\x00\x00\x41\x01\xd6\x00\x1a\x0b")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: br_on_non_null ref to type I32 expected a reference type, in function at index 0"
  )
}

assert.asyncTest(br_on_null());
assert.asyncTest(br_on_non_null());
