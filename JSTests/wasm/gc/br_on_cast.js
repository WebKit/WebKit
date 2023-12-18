//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testBrOnCastValidation() {
  assert.throws(
    () => compile(`
      (module
        (func (param anyref) (result i31ref)
          (br_on_cast 2 anyref i31ref (local.get 0))
          (br 0 (ref.null i31))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 7: br / br_if's target 2 exceeds control stack size 1, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
        (func (param anyref) (result i32)
          (block (result i32)
            (br_on_cast 0 anyref i31ref (local.get 0))
            (br 0 (i32.const 42)))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: branch's stack type is not a subtype of block's type branch target type. Stack value has type (ref null i31) but branch target expects a value of I32 at index 0, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
        (func (param anyref) (result externref)
          (block (result externref)
            (br_on_cast 0 anyref externref (local.get 0))
            (br 0 (ref.null extern)))))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: target heaptype was not a subtype of source heaptype for br_on_cast, in function at index 0"
  );

  compile(`
    (module
      (type (struct))
      (func (param (ref any)) (result (ref null 0))
        (ref.null 0))
      (func (param anyref) (result (ref null 0))
        (br_on_cast 0 anyref (ref null 0) (local.get 0))
        (call 0)))
  `),

  assert.throws(
    () => compile(`
      (module
        (func (param (ref extern)) (result (ref extern)) (local.get 0))
        (func (param externref) (result (ref extern))
          (br_on_cast 0 externref (ref extern) (local.get 0))
          (call 0)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: argument type mismatch in call, got (ref null extern), expected (ref extern), in function at index"
  );
}

function testBrOnCast() {
  {
    let m = instantiate(`
      (module
        (type (struct))
        (func (param anyref) (result i32)
          (block (result i32 i31ref)
            (br_on_cast 0 anyref i31ref (i32.const 42) (local.get 0))
            (br 0 (i32.const 7) (ref.null i31)))
          drop)
        (func (export "f1") (result i32)
          (call 0 (ref.i31 (i32.const 1))))
        (func (export "f2") (result i32)
          (call 0 (struct.new 0))))
    `);
    assert.eq(m.exports.f1(), 42);
    assert.eq(m.exports.f2(), 7);
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (func (param anyref) (result i32)
          (block (result i32 (ref any))
            (br_on_cast_fail 0 anyref i31ref (i32.const 42) (local.get 0))
            (br 0 (i32.const 7) (struct.new 0)))
          drop)
        (func (export "f1") (result i32)
          (call 0 (ref.i31 (i32.const 1))))
        (func (export "f2") (result i32)
          (call 0 (struct.new 0))))
    `);
    assert.eq(m.exports.f1(), 7);
    assert.eq(m.exports.f2(), 42);
  }

  {
    let m = instantiate(`
      (module
        (func (param anyref) (result i31ref)
          (local.get 0)
          (block (param anyref) (result i31ref)
            (br_on_cast 0 anyref (ref i31))
            drop
            (ref.i31 (i32.const 0))))
        (func (export "f1") (result i31ref)
          (call 0 (ref.i31 (i32.const 1))))
        (func (export "f2") (result i31ref)
          (call 0 (ref.null i31))))
    `);
    assert.eq(m.exports.f1(), 1);
    assert.eq(m.exports.f2(), 0);
  }
}

testBrOnCastValidation();
testBrOnCast();
