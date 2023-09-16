//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function testLongArray() {
    // This can be adjusted depending on the stack size
    let len = 4500;
    var elements = "";
    for(var i = 0; i < len; i++) {
        elements += "(i32.const " + i + ") ";
    }

    let moduleString = `(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec ` + len + ` ` + elements + `)
  )

  (func $get (param $i i32) (param $v (ref $vec)) (result i32)
    (array.get $vec (local.get $v) (local.get $i))
  )
  (func (export "get") (param $i i32) (result i32)
    (call $get (local.get $i) (call $new))
  )

  (func $len (param $v (ref array)) (result i32)
    (array.len (local.get $v))
  )
  (func (export "len") (result i32)
    (call $len (call $new))
  )
)`;
    let m = instantiate(moduleString);

    for (var i = 0; i < len; i++) {
        assert.eq(m.exports.get(i), i);
    }
    assert.throws(() => m.exports.get(len + 1), WebAssembly.RuntimeError, "Out of bounds array.get");
    assert.eq(m.exports.len(), len);
}

testLongArray();
