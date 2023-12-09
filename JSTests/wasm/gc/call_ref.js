//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testRefSubtyping() {
  // A call to a subtype should validate.
  instantiate(`
    (module
      (type (sub (func (param i32))))
      (type (sub 0 (func (param i32))))
      (global (ref 1) (ref.func 0))
      (func (type 1))
      (func (call_ref 0 (i32.const 3) (global.get 0)))
    )
  `);
}

function testArgSubtyping() {
  // Ensure that call_ref uses subtyping for arguments.
  instantiate(`
    (module
      (func (param eqref))
      (global (ref 0) (ref.func 0))
      (func (call_ref 0 (ref.i31 (i32.const 42)) (global.get 0)))
    )
  `);
}

function testTypeDefCheck() {
  // Non-func typedefs are invalid.
  assert.throws(
    () => instantiate(`
      (module
        (type (struct))
        (func (call_ref 0 (ref.null func)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: invalid type index (not a function signature) for call_ref, got 0, in function at index 0"
  );
}

testRefSubtyping();
testArgSubtyping();
testTypeDefCheck();
