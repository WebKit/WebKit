//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--useWebAssemblyExtendedConstantExpressions=false")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

async function testInvalidGCConstExprs() {
  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (global (export "g") (ref 0) (array.new 0))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 29: unknown init_expr opcode 251"
  );

  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (global (export "g") (ref 0) (array.new 0 (i32.const 42) (i32.const 10)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 30: init_expr should end with end, ended with 65"
  );

  assert.throws(
    () => compile(`
      (module
        (type (struct (field i32)))
        (global (export "g") (ref 0) (struct.new 0 (i32.const 42)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 31: init_expr should end with end, ended with 251"
  );

  assert.throws(
    () => compile(`
      (module
        (global (export "g") (ref i31) (ref.i31 (i32.const 555))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 21: init_expr should end with end, ended with 251"
  );
}

assert.asyncTest(testInvalidGCConstExprs());
