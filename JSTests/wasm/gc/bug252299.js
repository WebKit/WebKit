//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; ++i) {
        print(bytes.charCodeAt(i));
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function arrayGetUninitializedLocal() {
    // Test that array.get/set, struct.get/set and i31.get_<sx> on an uninitialized local
    // correctly trap null (i.e. that locals with reference types are initialized to
    // null)
    let m = instantiate(`(module
      (type $a (array (mut i32)))
      (type $s (struct (field $x (mut i32)) (field $y (mut i64))))
      (func (export "arrayGetNull")
        (local (ref null $a)) (drop (array.get $a (local.get 0) (i32.const 0)))
      )
      (func (export "arraySetNull")
        (local (ref null $a)) (array.set $a (local.get 0) (i32.const 0) (i32.const 0))
      )
      (func (export "structGetNull")
        (local (ref null $s)) (drop (struct.get $s $x (local.get 0)))
      )
      (func (export "structSetNull")
        (local (ref null $s)) (struct.set $s $x (local.get 0) (i32.const 0))
      )
      (func (export "i31GetNull")
        (local (ref null i31)) (drop (i31.get_s (local.get 0))))
      )`);


    for (var p of [["arrayGetNull", "array.get"],
                   ["arraySetNull", "array.set"],
                   ["structGetNull", "struct.get"],
                   ["structSetNull", "struct.set"],
                   ["i31GetNull", "i31.get_<sx>"]]) {
        assert.throws(() => m.exports[p[0]](), WebAssembly.RuntimeError, p[1] + " to a null reference");
    }
}

arrayGetUninitializedLocal();

