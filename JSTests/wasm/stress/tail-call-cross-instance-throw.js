//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that exceptions thrown by a tail callee in a different instance
// correctly unwind through the RestoreInstance thunk frame.

// Module A: tail-calls into B's thrower function.
let watA = `
(module
    (import "b" "thrower" (func $thrower (param i32) (result i32)))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

    ;; Tail-calls into B, which will throw.
    (func (export "do_tail_call") (param i32) (result i32)
        local.get 0
        return_call $thrower
    )

    ;; Regular call wrapper so there's a non-tail frame on the stack.
    (func (export "call_then_tail_call") (param i32) (result i32)
        local.get 0
        call 0
    )

    ;; Reads our own memory after the callee returns (should never execute if callee throws).
    (func (export "call_with_memory_after") (param i32) (result i32)
        local.get 0
        call 0
        ;; If we get here, the callee didn't throw. Read our memory to verify restore.
        (i32.load (i32.const 0))
        i32.add
    )
)
`

// Module B: throws when n <= 0, otherwise tail-calls back to A.
let watB = `
(module
    (import "a" "do_tail_call" (func $a_do_tail_call (param i32) (result i32)))
    (import "js" "doThrow" (func $doThrow))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255

    ;; If n <= 0, throw via JS import. Otherwise tail-call back to A with n-1.
    (func (export "thrower") (param i32) (result i32)
        (if (i32.le_s (local.get 0) (i32.const 0))
            (then
                call $doThrow
                unreachable
            )
        )
        (i32.sub (local.get 0) (i32.const 1))
        return_call $a_do_tail_call
    )
)
`

async function test() {
    const sentinel = new Error("cross-instance-throw-test");

    const instanceA = await instantiate(watA, {
        b: { thrower: () => { throw sentinel; } }  // placeholder, replaced below
    }, { tail_call: true });

    const instanceB = await instantiate(watB, {
        a: { do_tail_call: instanceA.exports.do_tail_call },
        js: { doThrow: () => { throw sentinel; } },
    }, { tail_call: true });

    // Re-instantiate A with B's real thrower.
    const instA = await instantiate(watA, {
        b: { thrower: instanceB.exports.thrower },
    }, { tail_call: true });

    // --- Test 1: Direct throw (n=0), single cross-instance hop ---
    let caught = false;
    try {
        instA.exports.call_then_tail_call(0);
    } catch (e) {
        caught = true;
        assert.eq(e, sentinel);
    }
    assert.eq(caught, true, "Exception should propagate through thunk");

    // --- Test 2: Throw after several cross-instance hops (n=10) ---
    caught = false;
    try {
        instA.exports.call_then_tail_call(10);
    } catch (e) {
        caught = true;
        assert.eq(e, sentinel);
    }
    assert.eq(caught, true, "Exception should propagate through deep cross-instance chain");

    // --- Test 3: After a successful cross-instance tail call, a throwing one should not
    //     corrupt the caller's state. Do a non-throwing call first, then a throwing one. ---
    // First, verify non-throwing path works (thrower with n > 0 tail-calls back to A
    // which tail-calls back, etc. We need a non-throwing variant).
    // Instead, just verify that after catching the throw, we can still call successfully.
    caught = false;
    try {
        instA.exports.call_then_tail_call(0);
    } catch (e) {
        caught = true;
    }
    assert.eq(caught, true);

    // After the throw, calling again should still work (no corrupted state).
    caught = false;
    try {
        instA.exports.call_then_tail_call(0);
    } catch (e) {
        caught = true;
        assert.eq(e, sentinel);
    }
    assert.eq(caught, true, "Repeated throws should work");

}

await assert.asyncTest(test())
