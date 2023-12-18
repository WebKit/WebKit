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

async function ref_as_non_null() {
  /*
  (module
    (type $t (func))
    (elem declare funcref (ref.func $foo))
    (func $foo)
    (func $f (result (ref null $t)) (ref.func $foo))
    (func (export "main") (result (ref $t))
      (ref.as_non_null (call $f)))
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8e\x80\x80\x80\x00\x03\x60\x00\x00\x60\x00\x01\x63\x00\x60\x00\x01\x64\x00\x03\x84\x80\x80\x80\x00\x03\x00\x01\x02\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x02\x09\x87\x80\x80\x80\x00\x01\x07\x70\x01\xd2\x00\x0b\x0a\x9b\x80\x80\x80\x00\x03\x82\x80\x80\x80\x00\x00\x0b\x84\x80\x80\x80\x00\x00\xd2\x00\x0b\x85\x80\x80\x80\x00\x00\x10\x01\xd4\x0b"));
  instance.exports.main();

  /*
  (module
    (type $t (func))
    (elem declare funcref (ref.func $foo))
    (func $foo)
    (func $f (result (ref null $t)) (ref.null $t))
    (func (export "main") (result (ref $t))
      (ref.as_non_null (call $f)))
  )
  */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8e\x80\x80\x80\x00\x03\x60\x00\x00\x60\x00\x01\x63\x00\x60\x00\x01\x64\x00\x03\x84\x80\x80\x80\x00\x03\x00\x01\x02\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x02\x09\x87\x80\x80\x80\x00\x01\x07\x70\x01\xd2\x00\x0b\x0a\x9b\x80\x80\x80\x00\x03\x82\x80\x80\x80\x00\x00\x0b\x84\x80\x80\x80\x00\x00\xd0\x00\x0b\x85\x80\x80\x80\x00\x00\x10\x01\xd4\x0b")).exports.main(),
    WebAssembly.RuntimeError,
    "ref.as_non_null to a null reference (evaluating 'func(...args)')"
  )
}

assert.asyncTest(ref_as_non_null());
