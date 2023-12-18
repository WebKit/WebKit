//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testValidation() {
  assert.throws(
    () => compile(`
      (module
        (func (result i32)
          (ref.eq (ref.null extern) (ref.null array))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.eq ref1 to type RefNull expected Eqref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  assert.throws(
    () => compile(`
      (module
        (func (result i32)
          (ref.eq (i32.const 42) (ref.null array))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: ref.eq ref1 to type I32 expected Eqref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );
}

function testRefEq() {
  assert.eq(
    instantiate(`
       (module
         (func (export "f") (result i32)
           (ref.eq (ref.null array) (ref.null struct))))
    `).exports.f(),
    1
  );

  assert.eq(
    instantiate(`
       (module
         (type (struct))
         (func (export "f") (result i32)
           (ref.eq (struct.new 0) (ref.null struct))))
    `).exports.f(),
    0
  );

  assert.eq(
    instantiate(`
       (module
         (type (struct))
         (func (export "f") (result i32) (local (ref null 0))
           (local.set 0 (struct.new 0))
           (ref.eq (local.get 0) (local.get 0))))
    `).exports.f(),
    1
  );

  assert.eq(
    instantiate(`
       (module
         (func (export "f") (result i32)
           (ref.eq (ref.i31 (i32.const 42)) (ref.i31 (i32.const 42)))))
    `).exports.f(),
    1
  );

  assert.eq(
    instantiate(`
       (module
         (type (struct))
         (func (export "f") (result i32)
           (ref.eq (struct.new 0) (struct.new 0))))
    `).exports.f(),
    0
  );
}

testValidation();
testRefEq();
