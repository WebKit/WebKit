//@ requireOptions("--useWasmSIMD=1", "--useWasmTailCalls=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $call_same_size (export "call_same_size") (param f64) (param f64) (result i64)
      (f32.const 1337) (f32x4.splat)
      (return_call $callee_same_size))
    (func $callee_same_size (param $i v128) (result i64)
      (i64.add (i64.trunc_f32_s (f32x4.extract_lane 3 (local.get $i))) (i64.const 42)))
    (func $call_same_size_with_stack (export "call_same_size_with_stack") (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (result i64)
      (f32.const 1337) (f32x4.splat)
      (f32.const 1) (f32x4.splat)
      (f32.const 2) (f32x4.splat)
      (f32.const 3) (f32x4.splat)
      (f32.const 4) (f32x4.splat)
      (f32.const 5) (f32x4.splat)
      (f32.const 6) (f32x4.splat)
      (f32.const 7) (f32x4.splat)
      (f32.const 8)
      (f32.const 9) (f32x4.splat)
      (return_call $callee_same_size_with_stack))
    (func $callee_same_size_with_stack (param $i0 v128) (param $i1 v128) (param $i2 v128) (param $i3 v128) (param $i4 v128) (param $i5 v128) (param $i6 v128) (param $i7 v128) (param $i8 f32) (param $i9 v128) (result i64)
      (i64.add (i64.trunc_f32_s (f32x4.extract_lane 3 (local.get $i9))) (i64.trunc_f32_s (local.get $i8))))
    
    (func $call_bigger_with_stack (export "call_bigger_with_stack") (result i64)
      (f32.const 1337) (f32x4.splat)
      (f32.const 1) (f32x4.splat)
      (f32.const 2) (f32x4.splat)
      (f32.const 3) (f32x4.splat)
      (f32.const 4) (f32x4.splat)
      (f32.const 5) (f32x4.splat)
      (f32.const 6) (f32x4.splat)
      (f32.const 7) (f32x4.splat)
      (f32.const 8)
      (f32.const 90) (f32x4.splat)
      (return_call $callee_bigger_with_stack))
    (func $callee_bigger_with_stack (param $i0 v128) (param $i1 v128) (param $i2 v128) (param $i3 v128) (param $i4 v128) (param $i5 v128) (param $i6 v128) (param $i7 v128) (param $i8 f32) (param $i9 v128) (result i64)
      (i64.add (i64.trunc_f32_s (f32x4.extract_lane 3 (local.get $i9))) (i64.trunc_f32_s (local.get $i8))))
    (func $call_smaller_with_stack (export "call_smaller_with_stack") (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64) (param f64)  (param f64) (param f64) (param f64) (param f64)  (param $i11 f32) (result i64)
      (f32.const 1337) (f32x4.splat)
      (f32.const 1) (f32x4.splat)
      (f32.const 2) (f32x4.splat)
      (f32.const 3) (f32x4.splat)
      (f32.const 4) (f32x4.splat)
      (f32.const 5) (f32x4.splat)
      (f32.const 6) (f32x4.splat)
      (f32.const 7) (f32x4.splat)
      (f32.const 8)
      (f32.const 90) (f32x4.splat)
      (return_call $callee_smaller_with_stack))
    (func $callee_smaller_with_stack (param $i0 v128) (param $i1 v128) (param $i2 v128) (param $i3 v128) (param $i4 v128) (param $i5 v128) (param $i6 v128) (param $i7 v128) (param $i8 f32) (param $i9 v128) (result i64)
      (i64.add (i64.trunc_f32_s (f32x4.extract_lane 3 (local.get $i9))) (i64.trunc_f32_s (local.get $i8))))
  )
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, tail_call: true, exceptions: true })
    const { call_same_size, call_same_size_with_stack, call_bigger_with_stack, call_smaller_with_stack } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(call_same_size(), 1337n + 42n)
        assert.eq(call_same_size_with_stack(), 9n + 8n)
        assert.eq(call_bigger_with_stack(), 90n + 8n)
        assert.eq(call_smaller_with_stack(), 90n + 8n)
    }
}

await assert.asyncTest(test())
