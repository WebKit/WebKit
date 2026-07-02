//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $call_same_size (export "call_same_size") (result i32)
      (i32.const 1337)
      (return_call $callee_same_size))
    (func $callee_same_size (param $i i32) (result i32)
      (i32.add (local.get $i) (i32.const 42)))
    (func $call_same_size_with_stack (export "call_same_size_with_stack") (param $i0 i32) (param $i1 i32) (param $i2 i32) (param $i3 i32) (param $i4 i32) (param $i5 i32) (param $i6 i32) (param $i7 i32) (param $i8 i32) (param $i9 i32) (result i32)
      (i32.const 1337)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
      (i32.const 7)
      (i32.const 8)
      (i32.const 9)
      (return_call $callee_same_size_with_stack))
    (func $callee_same_size_with_stack (param $i0 i32) (param $i1 i32) (param $i2 i32) (param $i3 i32) (param $i4 i32) (param $i5 i32) (param $i6 i32) (param $i7 i32) (param $i8 i32) (param $i9 i32) (result i32)
      (i32.add (local.get $i9) (local.get $i2)))
    
    (func $call_bigger_with_stack (export "call_bigger_with_stack") (result i32)
      (i32.const 1337)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
      (i32.const 7)
      (i32.const 8)
      (i32.const 90)
      (return_call $callee_bigger_with_stack))
    (func $callee_bigger_with_stack (param $i0 i32) (param $i1 i32) (param $i2 i32) (param $i3 i32) (param $i4 i32) (param $i5 i32) (param $i6 i32) (param $i7 i32) (param $i8 i32) (param $i9 i32) (result i32)
      (i32.add (local.get $i9) (local.get $i2)))
    (func $call_smaller_with_stack (export "call_smaller_with_stack") (param $i0 i32) (param $i1 i32) (param $i2 i32) (param $i3 i32) (param $i4 i32) (param $i5 i32) (param $i6 i32) (param $i7 i32) (param $i8 i32) (param $i9 i32) (param $i10 i32) (param $i11 i32) (result i32)
      (i32.const 1337)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
      (i32.const 7)
      (i32.const 8)
      (i32.const 90)
      (return_call $callee_smaller_with_stack))
    (func $callee_smaller_with_stack (param $i0 i32) (param $i1 i32) (param $i2 i32) (param $i3 i32) (param $i4 i32) (param $i5 i32) (param $i6 i32) (param $i7 i32) (param $i8 i32) (param $i9 i32) (result i32)
      (i32.add (local.get $i9) (local.get $i2)))
  )
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, tail_call: true, exceptions: true })
    const { call_same_size, call_same_size_with_stack, call_bigger_with_stack, call_smaller_with_stack } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(call_same_size(), 1337 + 42)
        assert.eq(call_same_size_with_stack(), 9 + 2)
        assert.eq(call_bigger_with_stack(), 90 + 2)
        assert.eq(call_smaller_with_stack(), 90 + 2)
    }
}

await assert.asyncTest(test())