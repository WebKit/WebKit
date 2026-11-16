//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that GC during a cross-instance tail call chain doesn't corrupt the
// RestoreInstanceCallee thunk frame. The thunk's Callee slot holds the
// RestoreInstanceCallee singleton and CodeBlock holds a wasmInstance pointer;
// both must survive GC scanning.

let watPing = `
(module
    (type $pp (func (param i32) (result i32)))
    (import "t" "table" (table 2 funcref))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\2a\\00\\00\\00")

    (func (export "ping") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then (i32.load (i32.const 0)))
            (else
                (i32.sub (local.get 0) (i32.const 1))
                (i32.const 1)
                (return_call_indirect (type $pp))
            )
        )
    )

    (func (export "start") (param i32) (result i32)
        (call 0 (local.get 0))
    )
)
`

let watPong = `
(module
    (type $pp (func (param i32) (result i32)))
    (import "t" "table" (table 2 funcref))
    (import "t" "triggerGC" (func $triggerGC))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\ff\\00\\00\\00")

    (func (export "pong") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then
                ;; Force a full GC while the thunk frame is on the stack.
                (call $triggerGC)
                (i32.load (i32.const 0))
            )
            (else
                (i32.sub (local.get 0) (i32.const 1))
                (i32.const 0)
                (return_call_indirect (type $pp))
            )
        )
    )
)
`

async function test() {
    const table = new WebAssembly.Table({ initial: 2, element: "funcref" });
    function triggerGC() {
        // Force a full collection. The thunk frame's Callee (RestoreInstanceCallee)
        // and CodeBlock (wasmInstance) must be scannable by the GC.
        $vm.gc();
    }

    const instPing = await instantiate(watPing, { t: { table } }, { tail_call: true });
    const instPong = await instantiate(watPong, { t: { table, triggerGC } }, { tail_call: true });
    table.set(0, instPing.exports.ping);
    table.set(1, instPong.exports.pong);

    // Single cross-instance hop + GC at base case.
    assert.eq(instPing.exports.start(1), 255);

    // Deeper chain — GC happens after several cross-instance tail calls.
    assert.eq(instPing.exports.start(11), 255);

    // Multiple rounds to stress GC interaction.
    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(instPing.exports.start(1), 255);
        assert.eq(instPing.exports.start(11), 255);
    }
}

await assert.asyncTest(test())
