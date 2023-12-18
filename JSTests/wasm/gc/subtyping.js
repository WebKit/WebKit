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

function testNone() {
  compile(`
    (module
      (type (struct))
      (global (ref null 0) (ref.null none)))
  `);

  compile(`
    (module
      (type (array i32))
      (global (ref null 0) (ref.null none)))
  `);

  compile(`
    (module
      (global (ref null array) (ref.null none)))
  `);

  compile(`
    (module
      (global (ref null struct) (ref.null none)))
  `);

  compile(`
    (module
      (global (ref null i31) (ref.null none)))
  `);

  compile(`
    (module
      (global (ref null eq) (ref.null none)))
  `);

  compile(`
    (module
      (global (ref null any) (ref.null none)))
  `);

  compile(`
    (module
      (global (ref null none) (ref.null none)))
  `);

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (global (ref null 0) (ref.null none)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 31: Global init_expr opcode of type RefNull doesn't match global's type RefNull"
  );

  assert.throws(
    () => compile(`
      (module
        (global (ref null func) (ref.null none)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: Global init_expr opcode of type RefNull doesn't match global's type RefNull"
  );

  assert.throws(
    () => compile(`
      (module
        (global (ref null nofunc) (ref.null none)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: Global init_expr opcode of type RefNull doesn't match global's type RefNull"
  );

  assert.throws(
    () => compile(`
      (module
        (global (ref null extern) (ref.null none)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: Global init_expr opcode of type RefNull doesn't match global's type RefNull"
  );

  assert.throws(
    () => compile(`
      (module
        (global (ref null noextern) (ref.null none)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: Global init_expr opcode of type RefNull doesn't match global's type RefNull"
  );
}

testNone();
