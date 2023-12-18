//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

// Test for https://bugs.webkit.org/show_bug.cgi?id=265721

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

// Define modules with recursive types (with cached unrollings)
let m0 = null;
{
  // m1
  compile(`
    (module
      (rec (type (array (ref null 0))))
      (global (ref array) (array.new_default 0 (i32.const 5)))
    )
  `);

  m0 = instantiate(`
    (module
      (rec (type (array (ref null 0))))
      (func (export "f") (param (ref any)) (result i32)
        (ref.test (ref 0) (local.get 0)))
      (global (ref array) (array.new_default 0 (i32.const 5)))
    )
  `);
}

let f = m0.exports.f;
// Try to collect m1, triggering type information cleanup.
fullGC();

// Even after cleanup, type expansion should succeed.
let m2 = instantiate(`
  (module
    (rec (type (array (ref null 0))))
    (global i32 (i32.const 2))
    (global (ref array) (array.new_default 0 (i32.const 5)))
  )
`);

m0;
