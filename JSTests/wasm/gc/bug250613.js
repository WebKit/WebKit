//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--slowPathAllocsBetweenGCs=8")

// Test for https://bugs.webkit.org/show_bug.cgi?id=250613
// Note: without the --slowPathAllocsBetweenGCs=8 flag, this test only fails approximately every 1 in 10 executions.

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


function testStructSet() {
    /*
     * Point(i64)
     *
     * (module
     *   (type $Point (struct (field $x (mut i64))))
     *   (func $doTest (param $p (ref $Point)) (result i64)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (i64.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result i64)
     *     (call $doTest
     *       (struct.new $Point (i64.const 0))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x01\x7e\x01\x60\x01\x64\x00\x01\x7e\x60\x00\x01\x7e\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x1c\x02\x10\x00\x20\x00\x42\x25\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x09\x00\x42\x00\xfb\x00\x00\x10\x00\x0b"));
    assert.eq(instance.exports.main() == 37, true);
}

testStructSet();
