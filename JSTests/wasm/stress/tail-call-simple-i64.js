//@ requireOptions("--useWasmTailCalls=true", "--maximumWasmCalleeSIzeForInlining=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $call_same_size (export "call_same_size") (result i64)
      (i64.const 600000001337)
      (return_call $callee_same_size))
    (func $callee_same_size (param $i i64) (result i64)
      (i64.add (local.get $i) (i64.const 42)))
    (func $call_same_size_with_stack (export "call_same_size_with_stack") (param $i0 i64) (param $i1 i64) (param $i2 i64) (param $i3 i64) (param $i4 i64) (param $i5 i64) (param $i6 i64) (param $i7 i64) (param $i8 i64) (param $i9 i64) (result i64)
      (i64.const 600000001337)
      (i64.const 1)
      (i64.const 2)
      (i64.const 3)
      (i64.const 4)
      (i64.const 5)
      (i64.const 6)
      (i64.const 7)
      (i64.const 8)
      (i64.const 40000000009)
      (return_call $callee_same_size_with_stack))
    (func $callee_same_size_with_stack (param $i0 i64) (param $i1 i64) (param $i2 i64) (param $i3 i64) (param $i4 i64) (param $i5 i64) (param $i6 i64) (param $i7 i64) (param $i8 i64) (param $i9 i64) (result i64)
      (i64.add (local.get $i9) (local.get $i2)))

    (func $call_bigger_with_stack (export "call_bigger_with_stack") (result i64)
      (i64.const 600000001337)
      (i64.const 1)
      (i64.const 2)
      (i64.const 3)
      (i64.const 4)
      (i64.const 5)
      (i64.const 6)
      (i64.const 7)
      (i64.const 8)
      (i64.const 500000000090)
      (return_call $callee_bigger_with_stack))
    (func $callee_bigger_with_stack (param $i0 i64) (param $i1 i64) (param $i2 i64) (param $i3 i64) (param $i4 i64) (param $i5 i64) (param $i6 i64) (param $i7 i64) (param $i8 i64) (param $i9 i64) (result i64)
      (i64.add (local.get $i9) (local.get $i2)))
    (func $call_smaller_with_stack (export "call_smaller_with_stack") (param $i0 i64) (param $i1 i64) (param $i2 i64) (param $i3 i64) (param $i4 i64) (param $i5 i64) (param $i6 i64) (param $i7 i64) (param $i8 i64) (param $i9 i64) (param $i10 i64) (param $i11 i64) (result i64)
      (i64.const 600000001337)
      (i64.const 1)
      (i64.const 2)
      (i64.const 3)
      (i64.const 4)
      (i64.const 5)
      (i64.const 6)
      (i64.const 7)
      (i64.const 8)
      (i64.const 500000000090)
      (return_call $callee_smaller_with_stack))
    (func $callee_smaller_with_stack (param $i0 i64) (param $i1 i64) (param $i2 i64) (param $i3 i64) (param $i4 i64) (param $i5 i64) (param $i6 i64) (param $i7 i64) (param $i8 i64) (param $i9 i64) (result i64)
      (i64.add (local.get $i9) (local.get $i2)))
  )
`
let badArgs = new Array(12).fill(0xAAAAn)

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, tail_call: true, exceptions: true })
    const { call_same_size, call_same_size_with_stack, call_bigger_with_stack, call_smaller_with_stack } = instance.exports


    for (let i = 0; i < 1000; ++i) {
        assert.eq(call_same_size.apply(null, badArgs), 600000001337n + 42n)
        assert.eq(call_same_size_with_stack.apply(null, badArgs), 40000000009n + 2n)
        assert.eq(call_bigger_with_stack.apply(null, badArgs), 500000000090n + 2n)
        assert.eq(call_smaller_with_stack.apply(null, badArgs), 500000000090n + 2n)
    }
}

await assert.asyncTest(test())