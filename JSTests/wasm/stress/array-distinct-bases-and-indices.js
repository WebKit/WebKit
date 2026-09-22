//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// All elements of one array element type share a version counter, so what keeps two of them apart
// is the rest of the key: the array they are read out of, and the index value. Neither read below
// may answer for the other.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $A (array (mut i32)))

    (func (export "distinctArrays") (param $count i32) (result i32)
        (local $p (ref null $A))
        (local $q (ref null $A))
        (local $i i32)
        (local $sum i32)
        (local.set $p (array.new_default $A (i32.const 4)))
        (local.set $q (array.new_default $A (i32.const 4)))
        (loop $loop
            (array.set $A (local.get $p) (i32.const 0) (local.get $i))
            (array.set $A (local.get $q) (i32.const 0) (i32.add (local.get $i) (i32.const 100)))
            (local.set $sum
                (i32.add (local.get $sum)
                    (i32.sub (array.get $A (local.get $q) (i32.const 0))
                             (array.get $A (local.get $p) (i32.const 0)))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))

    (func (export "distinctIndices") (param $count i32) (result i32)
        (local $a (ref null $A))
        (local $i i32)
        (local $sum i32)
        (local.set $a (array.new_default $A (i32.const 4)))
        (loop $loop
            (array.set $A (local.get $a) (i32.const 0) (local.get $i))
            (array.set $A (local.get $a) (i32.const 1) (i32.add (local.get $i) (i32.const 100)))
            (local.set $sum
                (i32.add (local.get $sum)
                    (i32.sub (array.get $A (local.get $a) (i32.const 1))
                             (array.get $A (local.get $a) (i32.const 0)))))
            (local.set $i (i32.add (local.get $i) (i32.const 1)))
            (br_if $loop (i32.lt_s (local.get $i) (local.get $count))))
        (local.get $sum))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { distinctArrays, distinctIndices } = instance.exports

    for (let i = 0; i < testLoopCount; ++i) {
        assert.eq(distinctArrays(10), 1000)
        assert.eq(distinctIndices(10), 1000)
    }
}

await assert.asyncTest(test())
