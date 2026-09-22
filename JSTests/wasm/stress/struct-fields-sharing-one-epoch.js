//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--maxB3WasmGCEpochSnapshotEntries=0")

// A function with enough fields and blocks makes B3 give every struct field one shared version
// counter instead of one each. The option above forces that on any function at all. Distinct fields
// still have to answer for themselves: sharing a counter may only cost reuse, so the two reads below
// must return what was stored into their own field.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $S (struct (field $x (mut i32)) (field $y (mut i32))))

    (func (export "test") (param $count i32) (result i32)
        (local $s (ref null $S))
        (local $i i32)
        (local $sum i32)
        (local.set $s (struct.new $S (i32.const 0) (i32.const 0)))
        (loop $loop
            (struct.set $S $x (local.get $s) (local.get $i))
            (struct.set $S $y (local.get $s) (i32.add (local.get $i) (i32.const 1)))
            (local.set $sum
                (i32.add (local.get $sum)
                    (i32.sub (struct.get $S $y (local.get $s))
                             (struct.get $S $x (local.get $s)))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { test } = instance.exports

    for (let i = 0; i < testLoopCount; ++i)
        assert.eq(test(10), 10)
}

await assert.asyncTest(test())
