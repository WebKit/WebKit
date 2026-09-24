//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// B3 forwards a wasm-GC read out of an access that dominates it. The three functions below cover
// what that has to get right: a packed field still narrows the value it forwards, a store to one
// field leaves another field's value alone, a loop body's store retires what was read before the
// loop, and a read at a join takes neither arm's store.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $S (struct (field $b (mut i8)) (field $w (mut i32))))

    (func (export "packedForward") (param $flag i32) (result i32)
        (local $s (ref null $S))
        (local.set $s (struct.new $S (i32.const 0) (i32.const 0)))
        (struct.set $S $b (local.get $s) (i32.const 0x1ff))
        (if (local.get $flag)
            (then (struct.set $S $w (local.get $s) (i32.const 1))))
        (struct.get_u $S $b (local.get $s)))

    (func (export "loopInvalidate") (param $count i32) (result i32)
        (local $s (ref null $S))
        (local $i i32)
        (local $sum i32)
        (local.set $s (struct.new $S (i32.const 0) (i32.const 0)))
        (struct.set $S $w (local.get $s) (i32.const 7))
        (loop $loop
            (local.set $sum (i32.add (local.get $sum) (struct.get $S $w (local.get $s))))
            (struct.set $S $w (local.get $s) (i32.add (local.get $i) (i32.const 100)))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))

    (func (export "joinNoForward") (param $flag i32) (result i32)
        (local $s (ref null $S))
        (local.set $s (struct.new $S (i32.const 0) (i32.const 0)))
        (if (local.get $flag)
            (then (struct.set $S $w (local.get $s) (i32.const 11)))
            (else (struct.set $S $w (local.get $s) (i32.const 22))))
        (struct.get $S $w (local.get $s)))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { packedForward, loopInvalidate, joinNoForward } = instance.exports

    for (let i = 0; i < testLoopCount; ++i) {
        assert.eq(packedForward(i & 1), 0xff)
        // 7 before the loop body's first store, then 100, 101, 102.
        assert.eq(loopInvalidate(4), 310)
        assert.eq(joinNoForward(i & 1), (i & 1) ? 11 : 22)
    }
}

await assert.asyncTest(test())
