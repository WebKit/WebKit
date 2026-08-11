//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (import "m" "callee_same_size" (func $callee_same_size (param $i f64) (result i64)))
    (import "m" "callee_same_size_with_stack" (func $callee_same_size_with_stack (param $i0 f64) (param $i1 f64) (param $i2 f64) (param $i3 f64) (param $i4 f64) (param $i5 f64) (param $i6 f64) (param $i7 f64) (param $i8 f64) (param $i9 f64) (result i64)))
    (import "m" "callee_bigger_with_stack" (func $callee_bigger_with_stack (param $i0 f64) (param $i1 f64) (param $i2 f64) (param $i3 f64) (param $i4 f64) (param $i5 f64) (param $i6 f64) (param $i7 f64) (param $i8 f64) (param $i9 f64) (result i64)))
    (import "m" "callee_smaller_with_stack" (func $callee_smaller_with_stack (param $i0 f64) (param $i1 f64) (param $i2 f64) (param $i3 f64) (param $i4 f64) (param $i5 f64) (param $i6 f64) (param $i7 f64) (param $i8 f64) (param $i9 f64) (result i64)))
    (func $call_same_size (export "call_same_size") (result i64)
      (f64.const 1337)
      (return_call $callee_same_size))
    (func $call_same_size_with_stack (export "call_same_size_with_stack") (param $i0 f64) (param $i1 f64) (param $i2 f64) (param $i3 f64) (param $i4 f64) (param $i5 f64) (param $i6 f64) (param $i7 f64) (param $i8 f64) (param $i9 f64) (result i64)
      (f64.const 1337)
      (f64.const 1)
      (f64.const 2)
      (f64.const 3)
      (f64.const 4)
      (f64.const 5)
      (f64.const 6)
      (f64.const 7)
      (f64.const 8)
      (f64.const 9)
      (return_call $callee_same_size_with_stack))
    (func $call_bigger_with_stack (export "call_bigger_with_stack") (result i64)
      (f64.const 1337)
      (f64.const 1)
      (f64.const 2)
      (f64.const 3)
      (f64.const 4)
      (f64.const 5)
      (f64.const 6)
      (f64.const 7)
      (f64.const 8)
      (f64.const 90)
      (return_call $callee_bigger_with_stack))
    (func $call_smaller_with_stack (export "call_smaller_with_stack") (param $i0 f64) (param $i1 f64) (param $i2 f64) (param $i3 f64) (param $i4 f64) (param $i5 f64) (param $i6 f64) (param $i7 f64) (param $i8 f64) (param $i9 f64) (param $i10 f64) (param $i11 f64) (result i64)
      (f64.const 1337)
      (f64.const 1)
      (f64.const 2)
      (f64.const 3)
      (f64.const 4)
      (f64.const 5)
      (f64.const 6)
      (f64.const 7)
      (f64.const 8)
      (f64.const 90)
      (return_call $callee_smaller_with_stack))
)
`

async function test() {
    const instance = await instantiate(wat, { m: {
      callee_same_size: (i) => { return BigInt(i) + 42n },
      callee_same_size_with_stack: (i0, i1, i2, i3, i4, i5, i6, i7, i8, i9) => { return BigInt(i9) + BigInt(i2) },
      callee_bigger_with_stack: (i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11) => { return BigInt(i9) + BigInt(i2) },
      callee_smaller_with_stack: (i0, i1, i2, i3, i4, i5, i6, i7, i8, i9) => { return BigInt(i9) + BigInt(i2) },
    } }, { simd: true, tail_call: true, exceptions: true })
    const { call_same_size, call_same_size_with_stack, call_bigger_with_stack, call_smaller_with_stack } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(call_same_size(), 1337n + 42n)
        assert.eq(call_same_size_with_stack(), 9n + 2n)
        assert.eq(call_same_size_with_stack(1.5, 2.5, 3.0, 4.0, 5.0, 6.0, 7.5, -8.5, -9.5, -10.5), 9n + 2n)
        assert.eq(call_bigger_with_stack(), 90n + 2n)
        assert.eq(call_smaller_with_stack(), 90n + 2n)
    }
}

await assert.asyncTest(test())
