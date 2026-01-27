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

async function nullability() {
  /*
  (module
    (type $sig (func))
    (import "env" "getFunc" (func $getFunc (result (ref func))))
    (func (export "callGetFunc") (result i32)
          (call $getFunc)
          (ref.test (ref $sig)))
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0d\x03\x60\x00\x01\x64\x70\x60\x00\x01\x7f\x60\x00\x00\x02\x0f\x01\x03\x65\x6e\x76\x07\x67\x65\x74\x46\x75\x6e\x63\x00\x00\x03\x02\x01\x01\x07\x0f\x01\x0b\x63\x61\x6c\x6c\x47\x65\x74\x46\x75\x6e\x63\x00\x01\x0a\x09\x01\x07\x00\x10\x00\xfb\x14\x02\x0b"), {
    env: {
      getFunc: () => null
    }
  });

  assert.throws(
    () => {
      for (let i = 0; i < 1000; i++) {
        instance.exports.callGetFunc();
      }
    },
    TypeError,
    "Host function incorrectly returned null for a nonnullable reference type"
  )
}

await assert.asyncTest(nullability());
