import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

function testRefAsNonNull() {
  let m = instantiate(`
    (module
      (type $t (func (result i32)))
      (elem declare func $f)
      (func $f (type $t) (i32.const 42))
      (func (export "nonnull") (result i32)
        (call_ref $t (ref.as_non_null (ref.func $f))))
      (func (export "null") (result i32)
        (call_ref $t (ref.as_non_null (ref.null $t))))
    )
  `);
  for (let i = 0; i < wasmTestLoopCount; i++) {
    assert.eq(m.exports.nonnull(), 42);
    assert.throws(() => m.exports.null(), WebAssembly.RuntimeError, "ref.as_non_null to a null reference");
  }
}

function testCallRef() {
  let m = instantiate(`
    (module
      (type $t (func (result i32)))
      (func $f (type $t) (i32.const 7))
      (global $g (ref $t) (ref.func $f))
      (func (export "nonnull") (result i32)
        (call_ref $t (global.get $g)))
      (func (export "null") (result i32)
        (call_ref $t (ref.null $t)))
    )
  `);
  for (let i = 0; i < wasmTestLoopCount; i++) {
    assert.eq(m.exports.nonnull(), 7);
    assert.throws(() => m.exports.null(), WebAssembly.RuntimeError, "null reference");
  }
}

function testThrowRef() {
  // Binary: wast-wrapper and in-tree wabt cannot encode (ref exn).
  // (module
  //   (tag $e)
  //   (func (export "nonnull")
  //     (block $caught (result (ref exn))
  //       (try_table (result (ref exn)) (catch_ref $e $caught)
  //         (throw $e)
  //         (unreachable)))
  //     (throw_ref))
  //   (func (export "null")
  //     (throw_ref (ref.null exn))))
  let m = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
    0x03, 0x03, 0x02, 0x00, 0x00, 0x0d, 0x03, 0x01, 0x00, 0x00, 0x07, 0x12, 0x02, 0x07,
    0x6e, 0x6f, 0x6e, 0x6e, 0x75, 0x6c, 0x6c, 0x00, 0x00, 0x04, 0x6e, 0x75, 0x6c, 0x6c,
    0x00, 0x01, 0x0a, 0x1a, 0x02, 0x12, 0x00, 0x02, 0x64, 0x69, 0x1f, 0x64, 0x69, 0x01,
    0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x0b, 0x0b, 0x0a, 0x0b, 0x05, 0x00, 0xd0, 0x69,
    0x0a, 0x0b
  ])));
  for (let i = 0; i < wasmTestLoopCount; i++) {
    assert.throws(() => m.exports.nonnull(), WebAssembly.Exception, "");
    assert.throws(() => m.exports.null(), WebAssembly.RuntimeError, "throw_ref on a null reference");
  }
}

testRefAsNonNull();
testCallRef();
testThrowRef();
