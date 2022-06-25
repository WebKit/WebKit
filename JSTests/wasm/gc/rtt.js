//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function testRttTypes() {
  /*
  (module
      (type $Point (struct (field i32) (field i32)))
      (func (param (rtt $Point)))
  )
  */
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0c\x02\x5f\x02\x7f\x00\x7f\x00\x60\x01\x68\x00\x00\x03\x02\x01\x01\x0a\x05\x01\x03\x00\x01\x0b");
}

function testRttCanon() {
  {
    /*
    (module
      (type $Point (struct (field $x i32) (field $y i32)))
      (func $foo (param (rtt $Point)) (result i32) (i32.const 37))
      (func (export "main")
        (drop
          (call $foo (rtt.canon $Point))
        )
      )
    )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x10\x03\x5f\x02\x7f\x00\x7f\x00\x60\x01\x68\x00\x01\x7f\x60\x00\x00\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x0f\x02\x04\x00\x41\x25\x0b\x08\x00\xfb\x30\x00\x10\x00\x1a\x0b"));
    instance.exports.main();
  }

  {
    /*
    (module
      (type $Point (struct (field $x i32) (field $y i32)))
      (func (export "main")
        (unreachable)
        (rtt.canon $Point)
      )
    )
    */
    new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x00\x00\x5f\x02\x7f\x00\x7f\x00\x03\x02\x01\x00\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x08\x01\x06\x00\x00\xfb\x30\x01\x0b"));
  }
}

testRttTypes();
testRttCanon();
