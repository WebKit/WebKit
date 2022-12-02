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

async function callFunctionFromTheSameInstance() {
  /*
  (module
    (elem declare funcref (ref.func $foo))
    (func $foo (param $x i32) (result i32)
      (i32.add (local.get $x)
               (i32.const 19)
      )
    )
    (func (export "main") (result i32)
      (call_ref 0 (i32.const 10) (ref.func $foo))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x01\x7f\x01\x7f\x60\x00\x01\x7f\x03\x03\x02\x00\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x09\x05\x01\x03\x00\x01\x00\x0a\x12\x02\x07\x00\x20\x00\x41\x13\x6a\x0b\x08\x00\x41\x0a\xd2\x00\x14\x00\x0b"));
  assert.eq(instance.exports.main(), 29);
}

async function callFunctionFromTheDifferentInstance() {
  let wat = `
  (module
    (func (export "bar") (param $x i32) (result i32)
      (i32.add (local.get $x)
               (i32.const 19))
    )
  )`;
  const barProvider = await instantiate(wat, {}, {reference_types: true});

  /*
  (module
    (import "exports" "bar" (func $bar (param i32) (result i32)))
    (elem declare funcref (ref.func $bar))
    (func (export "main") (result i32)
      (call_ref 0 (i32.const 10) (ref.func $bar))
    )
  )
  */
  let instance = new WebAssembly.Instance(
    module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x01\x7f\x01\x7f\x60\x00\x01\x7f\x02\x0f\x01\x07\x65\x78\x70\x6f\x72\x74\x73\x03\x62\x61\x72\x00\x00\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x09\x05\x01\x03\x00\x01\x00\x0a\x0a\x01\x08\x00\x41\x0a\xd2\x00\x14\x00\x0b"),
    barProvider);
  assert.eq(instance.exports.main(), 29);
}

async function callFunctionFromJS() {
  /*
  (module
    (import "exports" "bar" (func $bar (param i32) (result i32)))
    (elem declare funcref (ref.func $bar))
    (func (export "main") (result i32)
      (call_ref 0 (i32.const 10) (ref.func $bar))
    )
  )
  */
  let instance = new WebAssembly.Instance(
    module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x01\x7f\x01\x7f\x60\x00\x01\x7f\x02\x0f\x01\x07\x65\x78\x70\x6f\x72\x74\x73\x03\x62\x61\x72\x00\x00\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x09\x05\x01\x03\x00\x01\x00\x0a\x0a\x01\x08\x00\x41\x0a\xd2\x00\x14\x00\x0b"),
    {exports: {bar : x => x + 19}});
  assert.eq(instance.exports.main(), 29);
}

async function invalidTypeIndex() {
  /*
  (module
    (elem declare funcref (ref.func $foo))
    (func $foo (param $x i32) (result i32)
      (i32.add (local.get $x)
               (i32.const 19)
      )
    )
    (func (export "main") (result i32)
      (call_ref 1 (i32.const 10) (ref.func $foo))
    )
  )
  */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x01\x7f\x01\x7f\x60\x00\x01\x7f\x03\x03\x02\x00\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x09\x05\x01\x03\x00\x01\x00\x0a\x12\x02\x07\x00\x20\x00\x41\x13\x6a\x0b\x08\x00\x41\x0a\xd2\x00\x14\x01\x0b")),
    WebAssembly.CompileError,
    "invalid type index for call_ref value"
  );
}

assert.asyncTest(callFunctionFromTheSameInstance());
assert.asyncTest(callFunctionFromTheDifferentInstance());
assert.asyncTest(callFunctionFromJS());
assert.asyncTest(invalidTypeIndex());
