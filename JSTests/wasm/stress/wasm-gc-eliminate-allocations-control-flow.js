//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// Exercises eliminateWasmGCAllocations against control-flow graph shapes that produce
// Identity wrappers and Phi nodes for struct pointers: block results, if/else, select,
// and loops. The pass must look through Identity (foldIdentity) and must treat a pointer
// that flows into a Phi/select or an escaping position as observable (kept), while still
// removing the genuinely-dead ones. All results are numeric so a miscompile is caught.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $point (struct (field $x (mut i32)) (field $y (mut i32))))

    ;; The struct pointer flows out of a block as its result (an Identity wrapper), then is
    ;; read. Dead after CSE forwarding; result must be correct.
    (func (export "blockResult") (param $a i32) (result i32)
        (local $p (ref $point))
        (local.set $p
            (block (result (ref $point))
                (struct.new $point (local.get $a) (i32.const 5))))
        (struct.get $point $x (local.get $p)))

    ;; Two fresh structs on the two arms of an if; the selected one is read. Both are
    ;; non-escaping; the merge is a Phi of two allocations.
    (func (export "ifElse") (param $a i32) (param $c i32) (result i32)
        (local $p (ref $point))
        (local.set $p
            (if (result (ref $point)) (local.get $c)
                (then (struct.new $point (local.get $a) (i32.const 1)))
                (else (struct.new $point (i32.const 2) (local.get $a)))))
        (struct.get $point $x (local.get $p)))

    ;; select between two fresh structs, then read.
    (func (export "selectStruct") (param $a i32) (param $c i32) (result i32)
        (local $p (ref $point))
        (local.set $p
            (select (result (ref $point))
                (struct.new $point (local.get $a) (i32.const 0))
                (struct.new $point (i32.const 9) (i32.const 0))
                (local.get $c)))
        (struct.get $point $y (local.get $p)))

    ;; A struct freshly allocated and read inside each loop iteration (the common
    ;; steady-state shape). The loop carries an accumulator, not the pointer.
    (func (export "loopLocal") (param $n i32) (result i32)
        (local $i i32)
        (local $sum i32)
        (local $p (ref $point))
        (block $done
            (loop $loop
                (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
                (local.set $p (struct.new $point (local.get $i) (i32.const 3)))
                (local.set $sum
                    (i32.add (local.get $sum)
                        (i32.add (struct.get $point $x (local.get $p))
                                 (struct.get $point $y (local.get $p)))))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $loop)))
        (local.get $sum))

    ;; The pointer is loop-carried through a Phi (escapes the simple pattern): must not be
    ;; miscompiled. Returns the last-written x.
    (func (export "loopCarried") (param $n i32) (result i32)
        (local $i i32)
        (local $p (ref $point))
        (local.set $p (struct.new $point (i32.const -1) (i32.const 0)))
        (block $done
            (loop $loop
                (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
                (local.set $p (struct.new $point (local.get $i) (i32.const 0)))
                (local.set $i (i32.add (local.get $i) (i32.const 1)))
                (br $loop)))
        (struct.get $point $x (local.get $p)))
)
`;

function loopLocalExpected(n) {
    let sum = 0;
    for (let i = 0; i < n; ++i)
        sum += i + 3;
    return sum;
}

globalThis.testLoopCount ??= 10000;

async function test() {
    const instance = await instantiate(wat, {});
    const { blockResult, ifElse, selectStruct, loopLocal, loopCarried } = instance.exports;

    for (let i = 0; i < testLoopCount; ++i) {
        assert.eq(blockResult(i), i);
        assert.eq(ifElse(i, 1), i);
        assert.eq(ifElse(i, 0), 2);
        assert.eq(selectStruct(i, 1), 0);
        assert.eq(selectStruct(i, 0), 0);
        assert.eq(loopLocal(i % 20), loopLocalExpected(i % 20));
        assert.eq(loopCarried(i % 20 + 1), (i % 20 + 1) - 1);
    }
}

await assert.asyncTest(test());
