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

function testStructOfInts() {

  let m = instantiate(`
    (module
      (type $s (struct (field i32) (field i32) (field i32)))

      (func $new (result (ref $s))
         (struct.new $s (i32.const 5) (i32.const 17) (i32.const 0)))

      (func (export "get0") (result i32)
         (struct.get $s 0 (call $new)))
      (func (export "get1") (result i32)
         (struct.get $s 1 (call $new)))
      (func (export "get2") (result i32)
         (struct.get $s 2 (call $new))))`);

    assert.eq(m.exports.get0(), 5);
    assert.eq(m.exports.get1(), 17);
    assert.eq(m.exports.get2(), 0);
}

function testStructDeclaration() {

  let m = instantiate(`
    (module
      (type $a (array i32))
      (type $s (struct (field (ref $a)) (field (ref $a)) (field (ref $a))))

      (func $new (result (ref $s))
         (struct.new $s (array.new_default $a (i32.const 5))
                        (array.new_default $a (i32.const 17))
                        (array.new_default $a (i32.const 0))))

      (func (export "len0") (result i32)
         (struct.get $s 0 (call $new))
         (array.len))
      (func (export "len1") (result i32)
         (struct.get $s 1 (call $new))
         (array.len))
      (func (export "len2") (result i32)
         (struct.get $s 2 (call $new))
         (array.len)))`);

    assert.eq(m.exports.len0(), 5);
    assert.eq(m.exports.len1(), 17);
    assert.eq(m.exports.len2(), 0);
}

// for comparison
testStructOfInts();
testStructDeclaration();
