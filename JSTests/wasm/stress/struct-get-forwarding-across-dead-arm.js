//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")

// The store in deadArm's then-arm never runs because its condition folds to 0, so the second
// struct.get may reuse the first. In liveArm the store runs on odd iterations and must be seen by
// the second struct.get. In foldedArm the condition folds to non-zero, so the arm merges with the
// code after it and the second struct.get must read the stored value.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
  (type $S (struct (field (mut i32))))

  (func (export "make") (param $value i32) (result (ref $S))
    (struct.new $S (local.get $value)))

  (func $deadArm (param $s (ref $S)) (param $x i32) (result i32)
    (local $t i32)
    (local.set $t (struct.get $S 0 (local.get $s)))
    (if (i32.and (local.get $x) (i32.const 0))
      (then
        (struct.set $S 0 (local.get $s) (i32.add (local.get $t) (i32.const 1)))))
    (i32.add (local.get $t) (struct.get $S 0 (local.get $s))))

  (func $liveArm (param $s (ref $S)) (param $x i32) (result i32)
    (local $t i32)
    (local.set $t (struct.get $S 0 (local.get $s)))
    (if (i32.and (local.get $x) (i32.const 1))
      (then
        (struct.set $S 0 (local.get $s) (i32.add (local.get $t) (i32.const 1)))))
    (i32.add (local.get $t) (struct.get $S 0 (local.get $s))))

  (func $foldedArm (param $s (ref $S)) (param $x i32) (result i32)
    (local $t i32)
    (local.set $t (struct.get $S 0 (local.get $s)))
    (if (i32.or (local.get $x) (i32.const -1))
      (then
        (struct.set $S 0 (local.get $s) (i32.const 5))))
    (i32.add (local.get $t) (struct.get $S 0 (local.get $s))))

  (func (export "runDeadArm") (param $s (ref $S)) (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc) (call $deadArm (local.get $s) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "runLiveArm") (param $s (ref $S)) (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc) (call $liveArm (local.get $s) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "runFoldedArm") (param $s (ref $S)) (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc) (call $foldedArm (local.get $s) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))
)
`

async function test() {
    const instance = await instantiate(wat)
    const { make, runDeadArm, runLiveArm, runFoldedArm } = instance.exports

    const iterations = 1000

    // The field starts at 3 and every odd iteration stores the value it read plus one, so
    // iteration k reads v(k) = 3 + floor(k / 2) first.
    let liveExpected = 0
    for (let k = 0, v = 3; k < iterations; ++k) {
        liveExpected += (k & 1) ? 2 * v + 1 : 2 * v
        if (k & 1)
            ++v
    }

    // The first call reads 3 and every later one reads 5, and each returns that plus 5.
    const foldedExpected = 8 + 10 * (iterations - 1)

    // The calls inside each run tier their callee up, so a few runs are enough to check OMG code.
    for (let i = 0; i < 10; ++i) {
        assert.eq(runDeadArm(make(3), iterations), 6 * iterations)
        assert.eq(runLiveArm(make(3), iterations), liveExpected)
        assert.eq(runFoldedArm(make(3), iterations), foldedExpected)
    }
}

await assert.asyncTest(test())
