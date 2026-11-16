//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that ipintReloadMemory in the restore thunk correctly reloads the
// caller's memory base and bounds after a cross-instance tail call returns.
// The callee instance grows its own memory; after return through the thunk,
// the caller must still see its own (ungrown) memory correctly.

// Module A: calls B via tail call, then reads its own memory after return.
let watA = `
(module
    (import "b" "grow_and_return" (func $b_grow (param i32) (result i32)))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\2a\\00\\00\\00")  ;; memory[0] = 42

    ;; Tail-call into B which will grow B's memory, then return.
    (func (export "tail_call_b") (param i32) (result i32)
        local.get 0
        return_call $b_grow
    )

    ;; Regular call wrapper: calls tail_call_b, then reads A's memory to
    ;; verify it's still accessible with correct base/bounds.
    (func (export "call_then_read") (param i32) (result i32)
        (call 0 (local.get 0))
        drop
        ;; After return through the thunk, A's memory must be restored.
        (i32.load (i32.const 0))
    )
)
`

// Module B: grows its own memory, then returns a value.
let watB = `
(module
    (memory (export "mem") 1)
    (data (i32.const 0) "\\ff\\00\\00\\00")  ;; memory[0] = 255

    ;; Grow our memory by N pages, then return memory[0].
    (func (export "grow_and_return") (param i32) (result i32)
        ;; Grow B's memory
        (memory.grow (local.get 0))
        drop
        ;; Return B's memory[0]
        (i32.load (i32.const 0))
    )
)
`

async function test() {
    const instanceA = await instantiate(watA, {
        b: { grow_and_return: () => 0 }  // placeholder
    }, { tail_call: true });

    const instanceB = await instantiate(watB, {}, { tail_call: true });

    // Re-instantiate A with B's real export.
    const instA = await instantiate(watA, {
        b: { grow_and_return: instanceB.exports.grow_and_return }
    }, { tail_call: true });

    // tail_call_b(5) → B grows by 5 pages, returns 255.
    // After return through thunk, A's memory must be intact.
    assert.eq(instA.exports.call_then_read(5), 42);

    // Do it again — B's memory is now larger but A's should be unchanged.
    assert.eq(instA.exports.call_then_read(5), 42);

    // Grow by a large amount to really stress the memory base/bounds reload.
    assert.eq(instA.exports.call_then_read(100), 42);

    // Multiple rounds — pass 0 so memory.grow is a no-op; we're stressing tier-up
    // of the tail-call + thunk path, not repeated allocation.
    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(instA.exports.call_then_read(0), 42);
}

await assert.asyncTest(test())
