//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// A struct.set whose reference comes straight out of a ref.cast loses its trap check, because the
// cast is made to do the null check instead. B3's reduceStrength must not lose the store when it
// rewrites it that way: the read below reaches the same field through the supertype and has to
// observe 42, not the 1 that was there before.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $Base (sub (struct (field (mut i32)))))
    (type $Derived (sub $Base (struct (field (mut i32)) (field (mut i32)))))

    (func (export "test") (param $count i32) (result i32)
        (local $o (ref null $Derived))
        (local $p (ref null $Base))
        (local $any anyref)
        (local $i i32)
        (local $before i32)
        (local $sum i32)
        (local.set $o (struct.new $Derived (i32.const 0) (i32.const 0)))
        (local.set $p (local.get $o))
        (local.set $any (local.get $o))
        (loop $loop
            (struct.set $Base 0 (local.get $p) (i32.const 1))
            (local.set $before (struct.get $Base 0 (local.get $p)))
            (struct.set $Derived 0 (ref.cast (ref null $Derived) (local.get $any)) (i32.const 42))
            (local.set $sum
                (i32.add (local.get $sum)
                    (i32.sub (struct.get $Base 0 (local.get $p)) (local.get $before))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { test } = instance.exports

    for (let i = 0; i < testLoopCount; ++i)
        assert.eq(test(10), 41 * 10)
}

await assert.asyncTest(test())
