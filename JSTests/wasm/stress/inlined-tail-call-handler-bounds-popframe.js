//@ requireOptions("--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000", "--wasmFunctionIndexRangeToCompile=8:100")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Companion to inlined-tail-call-handler-bounds.js: exercises the
// popInlinedFrame branch of linkExceptionHandlers — when an inlined caller
// does a *real* (non-inlined) tail call. The bounds verified by these
// scenarios:
//
//   B7:  handlerStartFilter.begin = codeOrigin.firstInlineCSI()
//   B8:  handlerStartFilter.end   = codeOrigin.lastInlineCSI() + 1
//   B9:  handlerEndFilter.begin   = codeOrigin.firstInlineCSI()
//   B10: handlerEndFilter.end     = codeOrigin.lastInlineCSI() + 2
//   B11: depth-0 guard so a marker is processed once
//   B12/B13/B14: trim correctness with multiple removals
//
// To force a "real" (non-inlined) tail call we rely on
// --wasmFunctionIndexRangeToCompile=8:100. Functions 0–7 are kept out of
// OMG, so they cannot be inlined; functions 8+ go through OMG and the test
// driver inlines its callees normally.

let wat = `
(module
    (tag $tag)

    ;; Functions 0–7: kept out of OMG by the function-index range, so any
    ;; tail call to one of these from an inlinee is a real tail call,
    ;; which is what triggers popInlinedFrame.
    (func $thrower throw $tag)               ;; fn 0
    (func)                                   ;; fn 1
    (func)                                   ;; fn 2
    (func)                                   ;; fn 3
    (func)                                   ;; fn 4
    (func)                                   ;; fn 5
    (func)                                   ;; fn 6
    (func)                                   ;; fn 7

    ;; --- Scenarios start at fn 8 ---

    ;; H: caller (inlined into driver) does a single real tail call wrapped
    ;;    in try/catch. The catch must NOT fire — its frame is popped before
    ;;    the tail call. Exercises B7-B10 single-removal path.
    (func $callerH                           ;; fn 8
        try
            return_call $thrower
        catch $tag
            unreachable
        end
    )
    (func (export "H") (result i32)          ;; fn 9
        (try (result i32)
            (do
                call $callerH
                (i32.const -1)
            )
            (catch $tag (i32.const 80))
        )
    )

    ;; I: caller does a real tail call, plus has some code BEFORE the tail
    ;;    call inside the try. Exception thrown by the pre-tail-call regular
    ;;    call must still be caught by caller (B12 — left piece intact).
    (func $callerI (param i32)               ;; fn 10
        try
            local.get 0
            i32.eqz
            if
                call $thrower      ;; non-tail — caught here
            end
            return_call $thrower   ;; real tail call (popInlinedFrame)
        catch $tag
            ;; landing pad for the non-tail throw only
            nop
        end
    )
    (func (export "I0") (result i32)         ;; fn 11
        (try (result i32)
            (do
                (call $callerI (i32.const 0))
                (i32.const 81)
            )
            (catch $tag (i32.const -1))
        )
    )
    (func (export "I1") (result i32)         ;; fn 12
        (try (result i32)
            (do
                (call $callerI (i32.const 1))
                (i32.const -1)
            )
            (catch $tag (i32.const 82))
        )
    )

    ;; J: caller has TWO real tail calls in the same try (multiple
    ;;    popInlinedFrame markers in one inlinee). Exercises multi-removal
    ;;    and capacity sizing.
    (func $callerJ (param i32)               ;; fn 13
        try
            local.get 0
            i32.eqz
            if
                return_call $thrower
            end
            return_call $thrower
        catch $tag
            unreachable
        end
    )
    (func (export "J0") (result i32)         ;; fn 14
        (try (result i32)
            (do
                (call $callerJ (i32.const 0))
                (i32.const -1)
            )
            (catch $tag (i32.const 83))
        )
    )
    (func (export "J1") (result i32)         ;; fn 15
        (try (result i32)
            (do
                (call $callerJ (i32.const 1))
                (i32.const -1)
            )
            (catch $tag (i32.const 84))
        )
    )

    ;; K: grandparent catch wraps a regular call to the inlinee, which
    ;;    real-tail-calls $thrower. Grandparent's m_start < X and m_end > V
    ;;    so the popInlinedFrame filter must reject it (B7/B8 reject below,
    ;;    B9/B10 reject above) — grandparent's handler stays intact and
    ;;    catches the exception via normal propagation.
    (func $callerK                           ;; fn 16
        return_call $thrower
    )
    (func (export "K") (result i32)          ;; fn 17
        (try (result i32)
            (do
                call $callerK
                (i32.const -1)
            )
            (catch $tag (i32.const 85))
        )
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { H, I0, I1, J0, J1, K } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r;
        r = H();   if (r !== 80) throw new Error(`H=${r} expected 80`);
        r = I0();  if (r !== 81) throw new Error(`I0=${r} expected 81`);
        r = I1();  if (r !== 82) throw new Error(`I1=${r} expected 82`);
        r = J0();  if (r !== 83) throw new Error(`J0=${r} expected 83`);
        r = J1();  if (r !== 84) throw new Error(`J1=${r} expected 84`);
        r = K();   if (r !== 85) throw new Error(`K=${r} expected 85`);
    }
    // print("PASS: inlined-tail-call-handler-bounds-popframe");
}

await assert.asyncTest(test())
