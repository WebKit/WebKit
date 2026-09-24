//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// An immutable field is never written, so it gets no version counter of its own and every one of
// them shares the catch-all. Nothing but the key's heap then tells two of them apart, and nothing
// retires either: the reads below have to survive a store and a call, and still answer for their
// own field.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $S (struct (field $a i32) (field $b i32) (field $m (mut i32))))

    (func $clobber (param $s (ref null $S)) (param $v i32)
        (struct.set $S $m (local.get $s) (local.get $v)))

    (func (export "test") (param $count i32) (result i32)
        (local $s (ref null $S))
        (local $i i32)
        (local $sum i32)
        (local.set $s (struct.new $S (i32.const 3) (i32.const 5) (i32.const 0)))
        (loop $loop
            (struct.set $S $m (local.get $s) (local.get $i))
            (call $clobber (local.get $s) (local.get $i))
            (local.set $sum
                (i32.add (local.get $sum)
                    (i32.sub (struct.get $S $b (local.get $s))
                             (struct.get $S $a (local.get $s)))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { test } = instance.exports

    for (let i = 0; i < testLoopCount; ++i)
        assert.eq(test(10), 20)
}

await assert.asyncTest(test())
