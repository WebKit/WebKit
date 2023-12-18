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

// These are all binary tests because exception handling is now finalized and the GC reference intepreter does not support it.
function testExceptionsWithGC() {
  /*
    (module
      (type (func))
      (elem declare (ref 0) (ref.fun 0))
      (tag (param funcref))
      (func (throw 0 (ref.func 0)))
    )
  */
  new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x70\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0d\x83\x80\x80\x80\x00\x01\x00\x01\x09\x88\x80\x80\x80\x00\x01\x07\x64\x00\x01\xd2\x00\x0b\x0a\x8c\x80\x80\x80\x00\x01\x86\x80\x80\x80\x00\x00\xd2\x00\x08\x00\x0b"));

  /*
    (module
      (type (func))
      (tag (param (ref 0)))
      (func try catch 0 drop end)
    )
  */
  new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x64\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0d\x83\x80\x80\x80\x00\x01\x00\x01\x0a\x8e\x80\x80\x80\x00\x01\x88\x80\x80\x80\x00\x00\x06\x40\x07\x00\x1a\x0b\x0b"));
}


testExceptionsWithGC();
