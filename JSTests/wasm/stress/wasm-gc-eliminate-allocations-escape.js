//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// Exercises cases where a struct allocation must be KEPT (it escapes) and cases mixing
// structs with arrays (which this pass does not touch). If the pass wrongly removed an
// escaping allocation these results would be wrong or it would crash.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $point (struct (field $x (mut i32)) (field $y (mut i32))))
    (type $arr (array (mut (ref null $point))))

    (global $g (mut (ref null $point)) (ref.null $point))

    ;; Escapes into a global, then read back out: must be kept.
    (func (export "escapeGlobal") (param $a i32) (result i32)
        (global.set $g (struct.new $point (local.get $a) (i32.const 0)))
        (struct.get $point $x (global.get $g)))

    ;; Escapes by being returned from $make; the caller (returnStruct) reads a field. The
    ;; returned reference must stay valid, so $make's allocation must be kept.
    (func $make (param $a i32) (result (ref $point))
        (struct.new $point (local.get $a) (i32.mul (local.get $a) (i32.const 2))))
    (func (export "returnStruct") (param $a i32) (result i32)
        (local $p (ref $point))
        (local.set $p (call $make (local.get $a)))
        (i32.add (struct.get $point $x (local.get $p))
                 (struct.get $point $y (local.get $p))))

    ;; Escapes into an array element, then read back: struct kept, array untouched by pass.
    (func (export "escapeArray") (param $a i32) (result i32)
        (local $arr (ref $arr))
        (local.set $arr (array.new $arr (ref.null $point) (i32.const 4)))
        (array.set $arr (local.get $arr) (i32.const 2)
            (struct.new $point (local.get $a) (i32.const 0)))
        (struct.get $point $x (array.get $arr (local.get $arr) (i32.const 2))))

    ;; Escapes by being passed to a call: must be kept.
    (func $readX (param $p (ref $point)) (result i32)
        (struct.get $point $x (local.get $p)))
    (func (export "escapeCall") (param $a i32) (result i32)
        (call $readX (struct.new $point (local.get $a) (i32.const 0))))

    ;; A dead struct alongside a live array in the same function: the struct is removed, the
    ;; array is left alone and its element read is correct.
    (func (export "deadStructLiveArray") (param $a i32) (result i32)
        (local $dead (ref $point))
        (local $arr (ref $arr))
        (local.set $dead (struct.new $point (local.get $a) (local.get $a)))
        (local.set $arr (array.new $arr (ref.null $point) (i32.const 2)))
        (array.set $arr (local.get $arr) (i32.const 0)
            (struct.new $point (local.get $a) (i32.const 0)))
        (struct.get $point $x (array.get $arr (local.get $arr) (i32.const 0))))
)
`;

globalThis.testLoopCount ??= 10000;

async function test() {
    const instance = await instantiate(wat, {});
    const { escapeGlobal, returnStruct, escapeArray, escapeCall, deadStructLiveArray } = instance.exports;

    for (let i = 0; i < testLoopCount; ++i) {
        assert.eq(escapeGlobal(i), i);
        assert.eq(returnStruct(i), i + i * 2);
        assert.eq(escapeArray(i), i);
        assert.eq(escapeCall(i), i);
        assert.eq(deadStructLiveArray(i), i);
    }
}

await assert.asyncTest(test());
