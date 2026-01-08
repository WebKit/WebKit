//@ requireOptions("--jitPolicyScale=0", "--useOMGInlining=1", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=2147483647", "--wasmInliningBudget=1000000", "--wasmInliningMaximumDepth=10", "--wasmInliningMaximumCount=1000")
//@ skip unless $isOMGPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Verifies that linkExceptionHandlers' range carving produces correct behavior
// at every boundary. For each "B#" item in this checklist, at least one
// scenario below has a handler whose m_start or m_end sits exactly on the
// boundary (or as close as the wasm structure allows). Comparing the result
// against wasm semantics (without inlining) is the test: any off-by-one in
// the bound would yield a different result.
//
// Checklist of bounds (see Source/JavaScriptCore/wasm/WasmCallee.cpp
// in OptimizingJITCallee::linkExceptionHandlers):
//
//   InlinedTailCall removal (lines ~641-645):
//     B1: toRemove.begin = codeOrigin.firstInlineCSI()   (Z) — inlinee body start
//     B2: toRemove.end   = codeOrigin.lastInlineCSI()    (W) — inlinee body end (excl)
//     B3: handlerStart.begin = parent.firstInlineCSI()   (X) — parent body start (inc)
//     B4: handlerStart.end   = codeOrigin.firstInlineCSI() (Z) — handler m_start cutoff (excl)
//     B5: handlerEnd.begin   = codeOrigin.lastInlineCSI() + 1 (W+1) — handler m_end lower (inc)
//     B6: handlerEnd.end     = parent.lastInlineCSI() + 1 (V+1) — handler m_end upper (excl)
//
//   popInlinedFrame removal (lines ~650-651):
//     B7:  handlerStartFilter.begin = codeOrigin.firstInlineCSI()    (X)
//     B8:  handlerStartFilter.end   = codeOrigin.lastInlineCSI() + 1 (W+1)
//     B9:  handlerEndFilter.begin   = codeOrigin.firstInlineCSI()    (X)
//     B10: handlerEndFilter.end     = codeOrigin.lastInlineCSI() + 2 (W+2)
//
//   Depth-0 guard (line ~662):
//     B11: only the deepest enclosing codeOrigin processes a popped marker
//
//   Trim (lines ~708-714):
//     B12: oldEnd clamping (left-piece m_end)
//     B13: newStart clamping (right-piece m_start)
//     B14: capacity sized so newHandlers[currentIndex] stays valid (sub-case
//          when a single handler is split by multiple removals; multi-tail
//          scenarios exercise this).

let wat = `
(module
    (tag $tag)
    (func $thrower
        throw $tag
    )
    (func $notThrower (result i32)
        (i32.const 99)
    )

    ;; ---- A: try-catch IMMEDIATELY around an inlined return_call ----
    ;; Exercises B4 (handler m_start just below Z), B5 (m_end just above W).
    ;; Expected: tail-call semantics pop $callerA's frame, so its catch
    ;; must NOT trigger. Returns 0 only if catch wrongly fires.
    (func $callerA_body
        try
            return_call $thrower
        catch $tag
            unreachable
        end
    )
    (func (export "A") (result i32)
        (try (result i32)
            (do
                call $callerA_body
                (i32.const -1)
            )
            (catch $tag (i32.const 1))
        )
    )

    ;; ---- B: try-catch with code before/after; B3/B6 boundaries ----
    ;; Parent has try opening after some prologue and catch with a body
    ;; before the function returns. m_start gets close to X, m_end close to V.
    (func $callerB_body
        (drop (i32.const 1))
        (drop (i32.const 2))
        (drop (i32.const 3))
        try
            return_call $thrower
        catch $tag
            unreachable
        end
        (drop (i32.const 4))
        (drop (i32.const 5))
    )
    (func (export "B") (result i32)
        (try (result i32)
            (do
                call $callerB_body
                (i32.const -1)
            )
            (catch $tag (i32.const 2))
        )
    )

    ;; ---- C: inlinee has its own try-catch (entirely inside inlinee) ----
    ;; The inlinee's handler is fully inside [Z, W) and must NOT be trimmed.
    ;; If trimming filters incorrectly accept it (e.g. B4 broadened), this
    ;; handler would be split incorrectly and the inner catch could be missed.
    (func $innerC (result i32)
        try (result i32)
            call $thrower
            (i32.const 0)
        catch $tag
            (i32.const 42)
        end
    )
    (func $callerC (export "C") (result i32)
        return_call $innerC
    )

    ;; ---- D: grandparent catch around regular call to caller ----
    ;; Outer try-catch wraps a plain call to $callerD; $callerD return_calls
    ;; $thrower. The grandparent's handler m_start is below X and m_end above V,
    ;; so it must NOT be trimmed (B3/B6 filter rejects it).
    (func $callerD
        return_call $thrower
    )
    (func (export "D") (result i32)
        try (result i32)
            call $callerD
            (i32.const -1)
        catch $tag
            (i32.const 77)
        end
    )

    ;; ---- E: two tail calls in one try (multiple removals, one handler) ----
    ;; Exercises B14 (capacity), B12/B13 (trim left/right pieces) when a
    ;; single handler is split by multiple removals.
    (func $callerE_body (param i32)
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
    (func (export "E0") (result i32)
        (try (result i32)
            (do
                (call $callerE_body (i32.const 0))
                (i32.const -1)
            )
            (catch $tag (i32.const 50))
        )
    )
    (func (export "E1") (result i32)
        (try (result i32)
            (do
                (call $callerE_body (i32.const 1))
                (i32.const -1)
            )
            (catch $tag (i32.const 51))
        )
    )

    ;; ---- F: pre-tail-call throw caught by caller ----
    ;; Caller has try; inside it does a regular call (which throws) then a
    ;; tail call (which would not throw). The LEFT piece of the carved handler
    ;; must still cover the pre-tail-call CSIs (B12 correctness).
    (func $callerF_body (param i32) (result i32)
        try (result i32)
            local.get 0
            i32.eqz
            if
                call $thrower    ;; regular call -> caught here
            end
            return_call $notThrower
            (i32.const -1)
        catch $tag
            (i32.const 88)
        end
    )
    (func (export "F0") (result i32)
        (call $callerF_body (i32.const 0))
    )
    (func (export "F1") (result i32)
        (call $callerF_body (i32.const 1))
    )

    ;; ---- G: nested inlined tail calls (B's parent is also inlined) ----
    ;; G calls $g_outer (NormalFrame inlined). $g_outer return_calls $g_inner
    ;; (InlinedTailCall). $g_outer has a catch_all around the tail call: must
    ;; NOT catch. Tests parent-codeOrigin lookup with InlinedTailCall depth>=2.
    (func $g_inner
        call $thrower
    )
    (func $g_outer
        try
            return_call $g_inner
        catch $tag
            unreachable
        end
    )
    (func (export "G") (result i32)
        (try (result i32)
            (do
                call $g_outer
                (i32.const -1)
            )
            (catch $tag (i32.const 7))
        )
    )
)
`

async function test() {
    // Note: --useConcurrentJIT=0 ensures compilation is synchronous in the
    // execution loop, so the tier-up to OMG happens deterministically and
    // we know each call eventually hits the inlined version.
    const instance = await instantiate(wat, {}, { exceptions: true, tail_call: true })
    const { A, B, C, D, E0, E1, F0, F1, G } = instance.exports

    // Run each scenario many times to ensure OMG compilation kicks in.
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let r;
        r = A();   if (r !== 1)  throw new Error(`A=${r} expected 1`);
        r = B();   if (r !== 2)  throw new Error(`B=${r} expected 2`);
        r = C();   if (r !== 42) throw new Error(`C=${r} expected 42`);
        r = D();   if (r !== 77) throw new Error(`D=${r} expected 77`);
        r = E0();  if (r !== 50) throw new Error(`E0=${r} expected 50`);
        r = E1();  if (r !== 51) throw new Error(`E1=${r} expected 51`);
        r = F0();  if (r !== 88) throw new Error(`F0=${r} expected 88`);
        r = F1();  if (r !== 99) throw new Error(`F1=${r} expected 99`);
        r = G();   if (r !== 7)  throw new Error(`G=${r} expected 7`);
    }
    // print("PASS: inlined-tail-call-handler-bounds");
}

await assert.asyncTest(test())
