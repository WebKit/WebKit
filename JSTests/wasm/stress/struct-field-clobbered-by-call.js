//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--useOMGInlining=0")

// B3 gives a version counter only to the struct fields a function stores to itself; a field it
// merely reads shares the catch-all counter, which a call has to move. Inlining is off so that the
// callee's store stays out of the caller and $y really is a field the caller never writes.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $S (struct (field $x (mut i32)) (field $y (mut i32))))

    (func $writeY (param $s (ref null $S)) (param $v i32)
        (struct.set $S $y (local.get $s) (local.get $v)))

    (func (export "test") (param $count i32) (result i32)
        (local $s (ref null $S))
        (local $i i32)
        (local $sum i32)
        (local.set $s (struct.new $S (i32.const 0) (i32.const 0)))
        (loop $loop
            (struct.set $S $x (local.get $s) (local.get $i))
            (local.set $sum (i32.add (local.get $sum) (struct.get $S $y (local.get $s))))
            (call $writeY (local.get $s) (local.get $i))
            (local.set $sum (i32.add (local.get $sum) (struct.get $S $y (local.get $s))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { test } = instance.exports

    // Iteration i reads $y twice: what the previous call left, then what this call just wrote.
    let expected = 0
    for (let i = 0; i < 4; ++i)
        expected += (i ? i - 1 : 0) + i

    for (let i = 0; i < testLoopCount; ++i)
        assert.eq(test(4), expected)
}

await assert.asyncTest(test())
