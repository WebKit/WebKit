//@ requireOptions("--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000", "--wasmFunctionIndexRangeToCompile=8:100")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Mixed chains: a function reached by an inlined tail call (InlinedTailCall) that
// itself makes a *real* (non-inlined) tail call. That real tail call becomes a
// popInlinedFrame site whose ranges must be punched out of the InlinedTailCall
// inlinee's OWN handlers (site 2 in linkExceptionHandlers, which runs for
// InlinedTailCall codeOrigins too). Boundaries tested (site 2):
//   handlerStartFilter = [C.firstInlineCSI, C.lastInlineCSI + 1)
//   handlerEndFilter   = [C.firstInlineCSI, C.lastInlineCSI + 2)
//   toRemove           = each contiguous popInlinedFrame CSI range
// Functions 0-7 are kept out of OMG (--wasmFunctionIndexRangeToCompile=8:100) so
// tail calls to them are real, triggering popInlinedFrame. Each scenario returns
// the wasm (no-inline) semantics value; an off-by-one in a filter or the removed
// range flips the result.

let wat = `
(module
    (tag $t)
    (tag $mc)     ;; "mid's own catch wrongly fired"
    (tag $pre)    ;; pre-tail-call throw

    ;; fns 0-7: out of OMG range -> real (non-inlined) callees.
    (func $leaf  (throw $t))   ;; fn 0
    (func $leaf2 (throw $t))   ;; fn 1
    (func) (func) (func) (func) (func) (func)   ;; fns 2-7 padding

    ;; ---- M: outer inline-tail-calls mid (mid == InlinedTailCall); mid wraps a
    ;; REAL tail call to $leaf in try/catch. mid is tail-called away before the
    ;; call, so its catch must be hidden. Correct: $t propagates to root -> 1.
    (func $M_mid                                   ;; fn 8
        (try
            (do (return_call $leaf))
            (catch $t (throw $mc))))
    (func $M_outer (return_call $M_mid))            ;; fn 9
    (func (export "M") (result i32)                 ;; fn 10
        (try (result i32)
            (do (call $M_outer) (i32.const -1))
            (catch $t  (i32.const 1))
            (catch $mc (i32.const 2))))

    ;; ---- N: mid (InlinedTailCall) throws $pre BEFORE its real tail call; its own
    ;; handler's left piece must survive the popInlinedFrame carving and catch it.
    (func $N_mid (param i32)                        ;; fn 11
        (try
            (do
                (if (local.get 0) (then (throw $pre)))
                (return_call $leaf))
            (catch $pre (throw $mc))               ;; left piece: caught here
            (catch $t   (throw $t))))              ;; real-tail-call body: hidden
    (func $N_outer (param i32) (return_call $N_mid (local.get 0)))   ;; fn 12
    (func (export "N0") (result i32)                ;; fn 13: no pre throw -> 3
        (try (result i32)
            (do (call $N_outer (i32.const 0)) (i32.const -1))
            (catch $t  (i32.const 3))
            (catch $mc (i32.const 30))))
    (func (export "N1") (result i32)                ;; fn 14: pre throw -> 30
        (try (result i32)
            (do (call $N_outer (i32.const 1)) (i32.const -1))
            (catch $t  (i32.const 3))
            (catch $mc (i32.const 30))))

    ;; ---- P: mid (InlinedTailCall) has TWO real tail calls in one try (two
    ;; popInlinedFrame ranges -> two removals from one handler). Neither catch may
    ;; fire. Correct -> 4 / 5 depending on which branch runs.
    (func $P_mid (param i32)                        ;; fn 15
        (try
            (do
                (if (i32.eqz (local.get 0)) (then (return_call $leaf)))
                (return_call $leaf2))
            (catch $t (throw $mc))))
    (func $P_outer (param i32) (return_call $P_mid (local.get 0)))   ;; fn 16
    (func (export "P0") (result i32)                ;; fn 17
        (try (result i32)
            (do (call $P_outer (i32.const 0)) (i32.const -1))
            (catch $t  (i32.const 4))
            (catch $mc (i32.const 40))))
    (func (export "P1") (result i32)                ;; fn 18
        (try (result i32)
            (do (call $P_outer (i32.const 1)) (i32.const -1))
            (catch $t  (i32.const 5))
            (catch $mc (i32.const 40))))

    ;; ---- Q: grandparent (normal call) wraps a chain whose inlined-tail-called
    ;; frame does a real tail call that throws. Grandparent must catch -> 6.
    (func $Q_mid (return_call $leaf))               ;; fn 19
    (func $Q_outer (return_call $Q_mid))            ;; fn 20
    (func (export "Q") (result i32)                 ;; fn 21
        (try (result i32)
            (do (call $Q_outer) (i32.const -1))
            (catch $t (i32.const 6))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { M, N0, N1, P0, P1, Q } = instance.exports
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r;
        r = M();  if (r !== 1)  throw new Error(`M=${r} expected 1`);
        r = N0(); if (r !== 3)  throw new Error(`N0=${r} expected 3`);
        r = N1(); if (r !== 30) throw new Error(`N1=${r} expected 30`);
        r = P0(); if (r !== 4)  throw new Error(`P0=${r} expected 4`);
        r = P1(); if (r !== 5)  throw new Error(`P1=${r} expected 5`);
        r = Q();  if (r !== 6)  throw new Error(`Q=${r} expected 6`);
    }
}

await assert.asyncTest(test())
