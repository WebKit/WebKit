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

let iterations = 10;

function testIntFields() {

  // size = 41
  let m = instantiate(`
    (module
      (type $s (struct (field i32) (field i32)))

      (func $new (export "new") (result (ref $s))
         (struct.new $s (i32.const 1) (i32.const 5)))

      (func (export "len0") (result i32)
         (struct.get $s 0 (call $new)))
      (func (export "len1") (result i32)
         (struct.get $s 1 (call $new))))`);

    /*
      Without the fix for bug 252719, the following assertions will fail
      after the point when the $new function has been compiled but
      $len0 and $len1 haven't been yet, so the value for the struct's 1st
      field will be written to its 0th field.
    */
    for (var i = 0; i < iterations; i++) {
        assert.eq(m.exports.len0(), 1);
        assert.eq(m.exports.len1(), 5);
    }

    /*
      If the bug is fixed in Air but not B3 (or vice versa), the following
      assertion will fail in the wasm.b3 config because m2 is below the size
      threshold and so it will be compiled with Air, while m is above the size
      threshold and will be compiled with B3. The wrong result will be read
      because the backends will disagree on struct field offsets.
     */

    // size = 14
    let m2 =  instantiate(`(module
      (type $s (struct (field i32) (field i32)))

      (import "m" "new" (func $new  (result (ref $s))))
      (func (export "f") (result i32)
         (call $new)
         (struct.get $s 1)))`, { "m": { "new": m.exports["new"] }});

    assert.eq(m2.exports.f(), 5);
}

testIntFields();
