//@ requireOptions("--jitPolicyScale=0", "--useOMGInlining=1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Depth tests for the InlinedTailCall carving. When a throw happens N tail-call
// levels below a handler, that handler must be hidden if (and only if) its frame
// was tail-called away. The carving reaches a distant ancestor transitively: each
// inlined-tail-call frame carves its whole body (which contains all deeper frames)
// out of its immediate parent's handlers, so the chain of per-level removals hides
// a deep throw from every tail-called-away ancestor -- up to, but not past, the
// first frame that is still live (the first one whose call was NOT a tail call).
//
// HA/HB/HC place a handler at exactly one ancestor (all other inlinees handler-
// less) with the throw 3/2/1 tail levels below it: each must be hidden -> the
// throw propagates to the root. ALIVE* place the handler on a frame that made a
// normal (non-tail) call before the tail chain, so it is still live and MUST
// catch -- proving the carving stops at the first live frame and does not
// over-hide it.

let wat = `
(module
    (tag $t)
    (tag $m)      ;; "the single ancestor handler fired"

    (func $thrower (throw $t))

    ;; ---- HA: handler 3 tail-levels above the throw (topmost inlinee) ----
    (func $a3 (return_call $thrower))
    (func $a2 (return_call $a3))
    (func $a1 (try (do (return_call $a2)) (catch $t (throw $m))))
    (func (export "HA") (result i32)
        (try (result i32) (do (call $a1) (i32.const -1))
            (catch $t (i32.const 1))
            (catch $m (i32.const 90))))

    ;; ---- HB: handler 2 tail-levels above the throw ----
    (func $b3 (return_call $thrower))
    (func $b2 (try (do (return_call $b3)) (catch $t (throw $m))))
    (func $b1 (return_call $b2))
    (func (export "HB") (result i32)
        (try (result i32) (do (call $b1) (i32.const -1))
            (catch $t (i32.const 2))
            (catch $m (i32.const 90))))

    ;; ---- HC: handler 1 tail-level above the throw ----
    (func $c3 (try (do (return_call $thrower)) (catch $t (throw $m))))
    (func $c2 (return_call $c3))
    (func $c1 (return_call $c2))
    (func (export "HC") (result i32)
        (try (result i32) (do (call $c1) (i32.const -1))
            (catch $t (i32.const 3))
            (catch $m (i32.const 90))))

    ;; ---- ALIVE1: the handler frame makes a NORMAL call, then that callee tail-
    ;; chains to the throw. The handler frame is still live and MUST catch. ----
    (func $al1_bot (return_call $thrower))
    (func $al1_mid (return_call $al1_bot))
    (func $al1_top (try (do (call $al1_mid)) (catch $t (throw $m))))   ;; NORMAL call
    (func (export "ALIVE1") (result i32)
        (try (result i32) (do (call $al1_top) (i32.const -1))
            (catch $t (i32.const -1))    ;; over-hiding would land here
            (catch $m (i32.const 4))))   ;; correct: live frame catches

    ;; ---- ALIVE2: live frame is itself reached by a tail call, then makes a
    ;; normal call whose callee tail-chains to the throw. Still must catch. ----
    (func $al2_bot (return_call $thrower))
    (func $al2_mid (return_call $al2_bot))
    (func $al2_live (try (do (call $al2_mid)) (catch $t (throw $m))))  ;; NORMAL call
    (func $al2_entry (return_call $al2_live))                          ;; tail into live frame
    (func (export "ALIVE2") (result i32)
        (try (result i32) (do (call $al2_entry) (i32.const -1))
            (catch $t (i32.const -1))
            (catch $m (i32.const 5))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { HA, HB, HC, ALIVE1, ALIVE2 } = instance.exports
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r;
        r = HA();     if (r !== 1) throw new Error(`HA=${r} expected 1`);
        r = HB();     if (r !== 2) throw new Error(`HB=${r} expected 2`);
        r = HC();     if (r !== 3) throw new Error(`HC=${r} expected 3`);
        r = ALIVE1(); if (r !== 4) throw new Error(`ALIVE1=${r} expected 4`);
        r = ALIVE2(); if (r !== 5) throw new Error(`ALIVE2=${r} expected 5`);
    }
}

await assert.asyncTest(test())
