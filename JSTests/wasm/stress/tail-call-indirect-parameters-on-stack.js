//@ requireOptions("--useWasmTailCalls=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (type $f2_sig (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (table 1 funcref)
  (func $f1 (export "f1") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 7
    local.get 8
    local.get 9
    local.get 10
    i32.const 100
    i32.const 200
    i32.const 300
    i32.const 0
    (return_call_indirect (type $f2_sig))
  )
  (func $f2 (type $f2_sig)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 7
    local.get 8
    local.get 9
    local.get 10
    local.get 11
    local.get 12
    local.get 13
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
  )
  (elem (i32.const 0) $f2)
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, tail_call: true, exceptions: true })
    const { f1 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
      let result = f1(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
      if (result !== 655)
        throw new Error(result);
    }
}

assert.asyncTest(test())