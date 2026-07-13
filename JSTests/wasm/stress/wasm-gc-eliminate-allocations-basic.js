//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// Exercises B3's eliminateWasmGCAllocations (dead non-escaping struct removal) plus the
// reduceStrength ref.eq folding of fresh allocations. Everything runs in a hot loop so the
// OMG tier (where the pass runs) compiles and executes.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (type $point (struct (field $x (mut i32)) (field $y (mut i32))))
    (type $box (struct (field $p (mut (ref null $point)))))
    (type $node (struct (field $v (mut i32)) (field $next (mut (ref null $node)))))

    ;; Allocate a struct, initialize it, never read it: pure dead allocation.
    (func (export "deadStruct") (param $a i32) (param $b i32) (result i32)
        (local $p (ref $point))
        (local.set $p (struct.new $point (local.get $a) (local.get $b)))
        (i32.add (local.get $a) (local.get $b)))

    ;; Allocate, set fields, read them back: CSE forwards the reads, the allocation is dead,
    ;; and the returned value must still be correct.
    (func (export "readBack") (param $a i32) (param $b i32) (result i32)
        (local $p (ref $point))
        (local.set $p (struct.new $point (local.get $a) (local.get $b)))
        (i32.sub (struct.get $point $x (local.get $p))
                 (struct.get $point $y (local.get $p))))

    ;; The struct escapes into a longer-lived struct that is returned, so it must NOT be
    ;; removed and the field read through the escaped reference must be correct.
    (func (export "escapeIntoBox") (param $a i32) (result i32)
        (local $p (ref $point))
        (local $b (ref $box))
        (local.set $p (struct.new $point (local.get $a) (i32.const 7)))
        (local.set $b (struct.new $box (local.get $p)))
        (struct.get $point $x (struct.get $box $p (local.get $b))))

    ;; ref.eq of a fresh struct against itself is always true.
    (func (export "eqSelf") (param $a i32) (result i32)
        (local $p (ref $point))
        (local.set $p (struct.new $point (local.get $a) (local.get $a)))
        (ref.eq (local.get $p) (local.get $p)))

    ;; ref.eq of two distinct fresh structs is always false.
    (func (export "eqTwo") (param $a i32) (result i32)
        (ref.eq (struct.new $point (local.get $a) (local.get $a))
                (struct.new $point (local.get $a) (local.get $a))))

    ;; ref.eq of a fresh struct against null is always false (fresh is non-null).
    (func (export "eqNull") (param $a i32) (result i32)
        (ref.eq (struct.new $point (local.get $a) (local.get $a))
                (ref.null $point)))

    ;; Self-referential store: the allocation's own pointer is written into its own field
    ;; (child(0) is the base, so it stays a removal candidate), and it is never read. This
    ;; is the a.next = a shape; it must be removed without miscompiling.
    (func (export "selfRefDead") (param $a i32) (result i32)
        (local $n (ref $node))
        (local.set $n (struct.new $node (local.get $a) (ref.null $node)))
        (struct.set $node $next (local.get $n) (local.get $n))
        (local.get $a))

    ;; Same self-referential store, but a field is read back afterwards (CSE forwards the
    ;; read of $v). Still dead; result must be correct.
    (func (export "selfRefRead") (param $a i32) (result i32)
        (local $n (ref $node))
        (local.set $n (struct.new $node (local.get $a) (ref.null $node)))
        (struct.set $node $next (local.get $n) (local.get $n))
        (struct.get $node $v (local.get $n)))

    ;; Self-referential store AND the node escapes (returned via $makeNode); it must be kept
    ;; and the cycle followed one hop must read the right value.
    (func $makeNode (param $a i32) (result (ref $node))
        (local $n (ref $node))
        (local.set $n (struct.new $node (local.get $a) (ref.null $node)))
        (struct.set $node $next (local.get $n) (local.get $n))
        (local.get $n))
    (func (export "selfRefEscape") (param $a i32) (result i32)
        (local $n (ref $node))
        (local.set $n (call $makeNode (local.get $a)))
        ;; n.next.next.v -- follows the self-cycle twice, must equal a.
        (struct.get $node $v
            (struct.get $node $next
                (struct.get $node $next (local.get $n)))))
)
`;

globalThis.testLoopCount ??= 10000;

async function test() {
    const instance = await instantiate(wat, {});
    const { deadStruct, readBack, escapeIntoBox, eqSelf, eqTwo, eqNull, selfRefDead, selfRefRead, selfRefEscape } = instance.exports;

    for (let i = 0; i < testLoopCount; ++i) {
        assert.eq(deadStruct(i, 3), i + 3);
        assert.eq(readBack(i + 10, 4), i + 6);
        assert.eq(escapeIntoBox(i), i);
        assert.eq(eqSelf(i), 1);
        assert.eq(eqTwo(i), 0);
        assert.eq(eqNull(i), 0);
        assert.eq(selfRefDead(i), i);
        assert.eq(selfRefRead(i), i);
        assert.eq(selfRefEscape(i), i);
    }
}

await assert.asyncTest(test());
