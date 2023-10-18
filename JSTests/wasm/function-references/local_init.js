//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true")

import * as assert from "../assert.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

async function testLocalInit() {
  /*
   *  (module
   *    (func (param (ref extern)) (local (ref extern))
   *      (local.get 1)
   *      drop))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8e\x80\x80\x80\x00\x01\x88\x80\x80\x80\x00\x01\x01\x64\x6f\x20\x01\x1a\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   *  (module
   *    (func (param (ref extern)) (local (ref extern))
   *      (local.set 1 (local.get 0))
   *      (local.get 1)
   *      drop))
   */
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x92\x80\x80\x80\x00\x01\x8c\x80\x80\x80\x00\x01\x01\x64\x6f\x20\x00\x21\x01\x20\x01\x1a\x0b");

  /*
   *  (module
   *    (func (param (ref extern)) (local (ref extern))
   *      (local.tee 1 (local.get 0))
   *      (local.get 1)
   *      drop drop))
   */
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x93\x80\x80\x80\x00\x01\x8d\x80\x80\x80\x00\x01\x01\x64\x6f\x20\x00\x22\x01\x20\x01\x1a\x1a\x0b");

  // FIXME: this test behavior may change depending on https://github.com/WebAssembly/function-references/issues/98
  /*
   *  (module
   *    (func (param (ref extern)) (local (ref extern))
   *      (unreachable)
   *      (local.get 1)
   *      drop))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8f\x80\x80\x80\x00\x01\x89\x80\x80\x80\x00\x01\x01\x64\x6f\x00\x20\x01\x1a\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   *  (module
   *    (func (param (ref extern)) (local (ref extern))
   *      (local.set 1 (ref.null extern))))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x8f\x80\x80\x80\x00\x01\x89\x80\x80\x80\x00\x01\x01\x64\x6f\xd0\x6f\x21\x01\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: set_local to type Externref expected Externref, in function at index 0"
  );

  /*
   * (module
   *   (func (param (ref extern)) (local (ref extern))
   *     (block
   *       (local.set 1 (local.get 0))
   *       (local.get 1)
   *       drop)))
   */
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x01\x01\x64\x6f\x02\x40\x20\x00\x21\x01\x20\x01\x1a\x0b\x0b");

  /*
   * (module
   *   (func (param (ref extern)) (local (ref extern))
   *     (local.set 1 (local.get 0))
   *     (block
   *       (local.get 1)
   *       drop)))
   */
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x01\x01\x64\x6f\x20\x00\x21\x01\x02\x40\x20\x01\x1a\x0b\x0b"),

  /*
   * (module
   *   (func (param (ref extern)) (local (ref extern))
   *     (loop
   *       (local.set 1 (local.get 0))
   *       (local.get 1)
   *       drop)))
   */
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x01\x01\x64\x6f\x03\x40\x20\x00\x21\x01\x20\x01\x1a\x0b\x0b");

  /*
   * (module
   *   (func (param (ref extern)) (local (ref extern))
   *     (block
   *       (local.set 1 (local.get 0)))
   *     (local.get 1)
   *     drop))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x01\x01\x64\x6f\x02\x40\x20\x00\x21\x01\x0b\x20\x01\x1a\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   * (module
   *   (func (param (ref extern)) (local (ref extern))
   *     (loop
   *       (local.set 1 (local.get 0)))
   *     (local.get 1)
   *     drop))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x01\x01\x64\x6f\x03\x40\x20\x00\x21\x01\x0b\x20\x01\x1a\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   * (module
   *   (func (param (ref extern)) (local (ref extern))
   *     (i32.const 1)
   *     (if (then
   *           (local.set 1 (local.get 0))
   *           (local.get 1)
   *           drop)
   *         (else
   *           (local.get 1)
   *           drop))
   *     ))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0a\x9b\x80\x80\x80\x00\x01\x95\x80\x80\x80\x00\x01\x01\x64\x6f\x41\x01\x04\x40\x20\x00\x21\x01\x20\x01\x1a\x05\x20\x01\x1a\x0b\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   * (module
   *   (tag)
   *   (func (param (ref extern)) (local (ref extern))
   *     (try (do
   *            (local.set 1 (local.get 0))
   *            (local.get 1)
   *            drop))))
   */
   module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0d\x83\x80\x80\x80\x00\x01\x00\x00\x0a\x95\x80\x80\x80\x00\x01\x8f\x80\x80\x80\x00\x01\x01\x64\x6f\x06\x40\x20\x00\x21\x01\x20\x01\x1a\x0b\x0b"),

  /*
   * (module
   *   (tag)
   *   (func (param (ref extern)) (local (ref extern))
   *     (try (do
   *            (local.set 1 (local.get 0))
   *            (local.get 1)
   *            drop)
   *          (catch 0
   *            (local.get 1)
   *            drop))
   *     ))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0d\x83\x80\x80\x80\x00\x01\x00\x00\x0a\x9a\x80\x80\x80\x00\x01\x94\x80\x80\x80\x00\x01\x01\x64\x6f\x06\x40\x20\x00\x21\x01\x20\x01\x1a\x07\x00\x20\x01\x1a\x0b\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   * (module
   *   (tag)
   *   (func (param (ref extern)) (local (ref extern))
   *     (try (do
   *            (local.set 1 (local.get 0))
   *            (local.get 1)
   *            drop)
   *          (catch_all
   *            (local.get 1)
   *            drop))
   *     ))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0d\x83\x80\x80\x80\x00\x01\x00\x00\x0a\x99\x80\x80\x80\x00\x01\x93\x80\x80\x80\x00\x01\x01\x64\x6f\x06\x40\x20\x00\x21\x01\x20\x01\x1a\x19\x20\x01\x1a\x0b\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );

  /*
   * (module
   *   (tag)
   *   (func (param (ref extern)) (local (ref extern))
   *     (try (do
   *            (local.set 1 (local.get 0))
   *            (local.get 1)
   *            drop)
   *          (delegate 0
   *            (local.get 1)
   *            drop))
   *     ))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x64\x6f\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0d\x83\x80\x80\x80\x00\x01\x00\x00\x0a\x9a\x80\x80\x80\x00\x01\x94\x80\x80\x80\x00\x01\x01\x64\x6f\x06\x40\x20\x00\x21\x01\x20\x01\x1a\x18\x00\x20\x01\x1a\x0b\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: non-defaultable function local 1 is accessed before initialization, in function at index 0"
  );
}

assert.asyncTest(testLocalInit());
