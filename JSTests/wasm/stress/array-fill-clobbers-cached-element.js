//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// A constant-count array.fill lowers to a bulk-memory fill that B3's reduceStrength expands into
// plain stores. The expansion has to keep invalidating the element it overwrites, so the read
// below must see the fill rather than the value stored just before it.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $arr (array (mut i8)))

    (func (export "test") (param $count i32) (result i32)
        (local $a (ref null $arr))
        (local $i i32)
        (local $sum i32)
        (local.set $a (array.new_default $arr (i32.const 8)))
        (loop $loop
            (array.set $arr (local.get $a) (i32.const 0) (i32.const 7))
            (array.fill $arr (local.get $a) (i32.const 0) (i32.const 0) (i32.const 8))
            (local.set $sum
                (i32.add (local.get $sum)
                    (array.get_u $arr (local.get $a) (i32.const 0))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { test } = instance.exports

    for (let i = 0; i < testLoopCount; ++i)
        assert.eq(test(10), 0)
}

await assert.asyncTest(test())
