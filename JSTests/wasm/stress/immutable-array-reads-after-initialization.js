//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

let calls = 0;

const wat = `
(module
    (import "e" "observe" (func $observe))
    (type $A (array i32))

    (func (export "fixed") (param $a i32) (param $b i32) (param $index i32) (result i32)
        (local $array (ref $A))
        (local.set $array (array.new_fixed $A 3 (local.get $a) (local.get $b) (i32.const 7)))
        (call $observe)
        (i32.add
            (i32.add
                (array.get $A (local.get $array) (i32.const 0))
                (array.get $A (local.get $array) (i32.const 1)))
            (i32.add
                (array.get $A (local.get $array) (local.get $index))
                (array.get $A (local.get $array) (local.get $index)))))

    (func (export "filled") (param $value i32) (param $index i32) (result i32)
        (local $array (ref $A))
        (local.set $array (array.new $A (local.get $value) (i32.const 4)))
        (call $observe)
        (i32.sub
            (array.get $A (local.get $array) (local.get $index))
            (array.get $A (local.get $array) (i32.const 3))))
)
`

async function test() {
    const instance = await instantiate(wat, { e: { observe() { ++calls; } } })
    const { fixed, filled } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(fixed(i, 100, 2), i + 100 + 14)
        assert.eq(fixed(i, 100, 0), i + 100 + 2 * i)
        assert.eq(filled(i, i & 3), 0)
    }
    assert.eq(calls, 3 * wasmTestLoopCount)
}

await assert.asyncTest(test())
