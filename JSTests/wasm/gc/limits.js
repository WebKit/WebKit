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

function testLimits() {
    // 1,000,000 recursion group size, at the limit.
    assert.throws(
        () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x87\x80\x80\x80\x00\x01\x4e\xC0\x84\x3D\x00\x00")),
        WebAssembly.CompileError,
        "WebAssembly.Module doesn't parse at byte 20: 0th Type is non-Func, non-Struct, and non-Array 0 (evaluating 'new WebAssembly.Module(buffer)'"
    );

    // 1,000,001 recursion group size, above limit.
    assert.throws(
        () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x87\x80\x80\x80\x00\x01\x4e\xC1\x84\x3D\x00\x00")),
        WebAssembly.CompileError,
        "WebAssembly.Module doesn't parse at byte 19: number of types for recursion group at position 0 is too big 1000001 maximum 1000000 (evaluating 'new WebAssembly.Module(buffer)"
    );

    // 63 depth subtype, at limit.
    compile(`
      (module
        (type (sub (struct)))    (type (sub 0 (struct)))  (type (sub 1 (struct)))  (type (sub 2 (struct)))
        (type (sub 3 (struct)))  (type (sub 4 (struct)))  (type (sub 5 (struct)))  (type (sub 6 (struct)))
        (type (sub 7 (struct)))  (type (sub 8 (struct)))  (type (sub 9 (struct)))  (type (sub 10 (struct)))
        (type (sub 11 (struct))) (type (sub 12 (struct))) (type (sub 13 (struct))) (type (sub 14 (struct)))
        (type (sub 15 (struct))) (type (sub 16 (struct))) (type (sub 17 (struct))) (type (sub 18 (struct)))
        (type (sub 19 (struct))) (type (sub 20 (struct))) (type (sub 21 (struct))) (type (sub 22 (struct)))
        (type (sub 23 (struct))) (type (sub 24 (struct))) (type (sub 25 (struct))) (type (sub 26 (struct)))
        (type (sub 27 (struct))) (type (sub 28 (struct))) (type (sub 29 (struct))) (type (sub 30 (struct)))
        (type (sub 31 (struct))) (type (sub 32 (struct))) (type (sub 33 (struct))) (type (sub 34 (struct)))
        (type (sub 35 (struct))) (type (sub 36 (struct))) (type (sub 37 (struct))) (type (sub 38 (struct)))
        (type (sub 39 (struct))) (type (sub 40 (struct))) (type (sub 41 (struct))) (type (sub 42 (struct)))
        (type (sub 43 (struct))) (type (sub 44 (struct))) (type (sub 45 (struct))) (type (sub 46 (struct)))
        (type (sub 47 (struct))) (type (sub 48 (struct))) (type (sub 49 (struct))) (type (sub 50 (struct)))
        (type (sub 51 (struct))) (type (sub 52 (struct))) (type (sub 53 (struct))) (type (sub 54 (struct)))
        (type (sub 55 (struct))) (type (sub 56 (struct))) (type (sub 57 (struct))) (type (sub 58 (struct)))
        (type (sub 59 (struct))) (type (sub 60 (struct))) (type (sub 61 (struct))) (type (sub 62 (struct)))
      )
    `);

    assert.throws(
        () => compile(`
          (module
            (type (sub (struct)))    (type (sub 0 (struct)))  (type (sub 1 (struct)))  (type (sub 2 (struct)))
            (type (sub 3 (struct)))  (type (sub 4 (struct)))  (type (sub 5 (struct)))  (type (sub 6 (struct)))
            (type (sub 7 (struct)))  (type (sub 8 (struct)))  (type (sub 9 (struct)))  (type (sub 10 (struct)))
            (type (sub 11 (struct))) (type (sub 12 (struct))) (type (sub 13 (struct))) (type (sub 14 (struct)))
            (type (sub 15 (struct))) (type (sub 16 (struct))) (type (sub 17 (struct))) (type (sub 18 (struct)))
            (type (sub 19 (struct))) (type (sub 20 (struct))) (type (sub 21 (struct))) (type (sub 22 (struct)))
            (type (sub 23 (struct))) (type (sub 24 (struct))) (type (sub 25 (struct))) (type (sub 26 (struct)))
            (type (sub 27 (struct))) (type (sub 28 (struct))) (type (sub 29 (struct))) (type (sub 30 (struct)))
            (type (sub 31 (struct))) (type (sub 32 (struct))) (type (sub 33 (struct))) (type (sub 34 (struct)))
            (type (sub 35 (struct))) (type (sub 36 (struct))) (type (sub 37 (struct))) (type (sub 38 (struct)))
            (type (sub 39 (struct))) (type (sub 40 (struct))) (type (sub 41 (struct))) (type (sub 42 (struct)))
            (type (sub 43 (struct))) (type (sub 44 (struct))) (type (sub 45 (struct))) (type (sub 46 (struct)))
            (type (sub 47 (struct))) (type (sub 48 (struct))) (type (sub 49 (struct))) (type (sub 50 (struct)))
            (type (sub 51 (struct))) (type (sub 52 (struct))) (type (sub 53 (struct))) (type (sub 54 (struct)))
            (type (sub 55 (struct))) (type (sub 56 (struct))) (type (sub 57 (struct))) (type (sub 58 (struct)))
            (type (sub 59 (struct))) (type (sub 60 (struct))) (type (sub 61 (struct))) (type (sub 62 (struct)))
            (type (sub 63 (struct)))
          )
        `),
        WebAssembly.CompileError,
        "WebAssembly.Module doesn't parse at byte 339: subtype depth for Type section's 64th signature exceeded the limits of 63"
    );
}

testLimits();
