//@ requireOptions("--jitPolicyScale=0", "--useOMGInlining=1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Companion to inlined-tail-call-handler-bounds.js, for chains where an inlined
// tail call's *immediate* parent is itself an inlined tail call (tail-in-tail,
// up to 4 inlinees deep). This exercises the InlinedTailCall removal in
// linkExceptionHandlers when parent is InlinedTailCall, so its bounds are
// computed from a tail-called parent's codeOrigin. Boundaries
// (site 1, WasmCallee.cpp OptimizingJITCallee::linkExceptionHandlers):
//   toRemove     = [C.firstInlineCSI, C.lastInlineCSI)
//   handlerStart = [parent.firstInlineCSI, C.firstInlineCSI)
//   handlerEnd   = [C.lastInlineCSI + 1, parent.lastInlineCSI + 1)
// where parent is discovered with getCodeOrigin(C.firstInlineCSI, depth 1).
// Each scenario returns a value that only matches wasm (no-inline) semantics
// when the carving is correct at every boundary; an off-by-one flips the result.

let wat = `
(module
    (tag $t)
    (tag $tw) (tag $tx) (tag $ty) (tag $tz)   ;; per-frame "wrongly caught" markers
    (tag $pre)                                 ;; pre-tail-call throw marker

    ;; ---- A: 4-deep chain; every frame wraps ONLY its tail call. z throws $t.
    ;; Each catch must be hidden (its frame tail-called away). Correct: propagate
    ;; to root -> 1. A per-frame value shows which handler wrongly fired.
    (func $A_z (throw $t))
    (func $A_y (try (do (return_call $A_z)) (catch $t (throw $ty))))
    (func $A_x (try (do (return_call $A_y)) (catch $t (throw $tx))))
    (func $A_w (try (do (return_call $A_x)) (catch $t (throw $tw))))
    (func (export "A") (result i32)
        (try (result i32)
            (do (call $A_w) (i32.const -1))
            (catch $t  (i32.const 1))
            (catch $tw (i32.const 10))
            (catch $tx (i32.const 20))
            (catch $ty (i32.const 30))))

    ;; ---- B: like A but with code before/after each tail call, so each frame's
    ;; try/catch m_start approaches parent.first (X) and m_end approaches
    ;; parent.last+1 (V+1). Tests handlerStart.begin and handlerEnd.end at nested
    ;; levels. Still must propagate -> 2.
    (func $B_z (throw $t))
    (func $B_y (drop (i32.const 1)) (try (do (return_call $B_z)) (catch $t (throw $ty))) (drop (i32.const 2)))
    (func $B_x (drop (i32.const 1)) (drop (i32.const 2)) (try (do (return_call $B_y)) (catch $t (throw $tx))) (drop (i32.const 3)))
    (func $B_w (drop (i32.const 1)) (try (do (return_call $B_x)) (catch $t (throw $tw))) (drop (i32.const 2)) (drop (i32.const 3)))
    (func (export "B") (result i32)
        (try (result i32)
            (do (call $B_w) (i32.const -1))
            (catch $t  (i32.const 2))
            (catch $tw (i32.const 10))
            (catch $tx (i32.const 20))
            (catch $ty (i32.const 30))))

    ;; ---- C: a middle (inlined-tail-called) frame throws BEFORE its tail call,
    ;; caught by its own handler. That handler's left piece (before the carved
    ;; tail-call body) must survive. Tests toRemove.begin (Z) not extending left.
    ;; $C_y throws $pre before return_call $C_z; its own catch handles it -> 42.
    (func $C_z (throw $t))
    (func $C_y (param i32)
        (try
            (do
                (if (local.get 0) (then (throw $pre)))
                (return_call $C_z))
            (catch $pre (throw $ty))       ;; pre-call throw: caught here (left piece)
            (catch $t   (throw $tz))))     ;; tail-call body: must be hidden
    (func $C_x (param i32) (return_call $C_y (local.get 0)))
    (func $C_w (param i32) (return_call $C_x (local.get 0)))
    (func (export "C0") (result i32)   ;; no pre-call throw: z's $t propagates -> 3
        (try (result i32)
            (do (call $C_w (i32.const 0)) (i32.const -1))
            (catch $t  (i32.const 3))
            (catch $ty (i32.const 30))
            (catch $tz (i32.const 31))))
    (func (export "C1") (result i32)   ;; pre-call throw: $C_y's left-piece catch fires -> 30
        (try (result i32)
            (do (call $C_w (i32.const 1)) (i32.const -1))
            (catch $t  (i32.const 3))
            (catch $ty (i32.const 30))
            (catch $tz (i32.const 31))))

    ;; ---- D: an inner inlinee (reached through a tail-in-tail chain) has its OWN
    ;; try/catch entirely inside its body; it must NOT be carved and must catch.
    (func $D_inner (result i32)
        (try (result i32)
            (do (throw $t) (i32.const -1))
            (catch $t (i32.const 42))))    ;; inner catch must survive -> 42
    (func $D_z (result i32) (return_call $D_inner))
    (func $D_y (result i32) (return_call $D_z))
    (func $D_w (export "D") (result i32) (return_call $D_y))

    ;; ---- E: grandparent (normal-call) catch around a tail-in-tail chain that
    ;; throws. The normal-frame handler must catch via propagation -> 5.
    (func $E_z (throw $t))
    (func $E_y (return_call $E_z))
    (func $E_x (return_call $E_y))
    (func $E_w (return_call $E_x))
    (func (export "E") (result i32)
        (try (result i32)
            (do (call $E_w) (i32.const -1))
            (catch $t (i32.const 5))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { A, B, C0, C1, D, E } = instance.exports
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r;
        r = A();  if (r !== 1)  throw new Error(`A=${r} expected 1`);
        r = B();  if (r !== 2)  throw new Error(`B=${r} expected 2`);
        r = C0(); if (r !== 3)  throw new Error(`C0=${r} expected 3`);
        r = C1(); if (r !== 30) throw new Error(`C1=${r} expected 30`);
        r = D();  if (r !== 42) throw new Error(`D=${r} expected 42`);
        r = E();  if (r !== 5)  throw new Error(`E=${r} expected 5`);
    }
}

await assert.asyncTest(test())
