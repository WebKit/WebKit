//@ requireOptions("--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that Error().stack captured during a cross-instance tail call chain
// does not include the RestoreInstanceCallee thunk frame.

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
    (import "t" "captureStack" (func $captureStack))
    (memory (export "mem") 1)
    (data (i32.const 0) "\\ff\\00\\00\\00")

    (func (export "pong") (param i32) (result i32)
        (if (result i32) (i32.le_s (local.get 0) (i32.const 0))
            (then (call $captureStack) (i32.load (i32.const 0)))
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
    let capturedStack = null;
    function captureStack() {
        capturedStack = new Error().stack;
    }

    const instPing = await instantiate(watPing, { t: { table } }, { tail_call: true });
    const instPong = await instantiate(watPong, { t: { table, captureStack } }, { tail_call: true });
    table.set(0, instPing.exports.ping);
    table.set(1, instPong.exports.pong);

    // Single cross-instance hop: start -> ping(1) -> pong(0) -> captureStack
    instPing.exports.start(1);
    assert.truthy(capturedStack !== null, "Stack should have been captured");

    // The stack should contain wasm-function frames but NOT any restore/thunk artifacts.
    // RestoreInstanceCallee has index 0xBADBADBA = 3131961274.
    let lines = capturedStack.split("\n");
    for (let line of lines) {
        assert.falsy(line.includes("3131961274"), "Stack should not contain RestoreInstanceCallee index: " + line);
        assert.falsy(line.includes("restoreInstance"), "Stack should not contain restoreInstance: " + line);
        assert.falsy(line.includes("restore_instance"), "Stack should not contain restore_instance: " + line);
    }

    // Should see wasm-function entries in the trace.
    let hasWasmFrame = lines.some(l => l.includes("wasm-function"));
    assert.truthy(hasWasmFrame, "Stack should contain wasm-function frames");

    // Deep chain: start -> ping(11) -> pong(10) -> ... -> pong(0) -> captureStack
    capturedStack = null;
    instPing.exports.start(11);
    assert.truthy(capturedStack !== null, "Stack should have been captured (deep)");

    lines = capturedStack.split("\n");
    for (let line of lines) {
        assert.falsy(line.includes("3131961274"), "Deep stack should not contain RestoreInstanceCallee index: " + line);
        assert.falsy(line.includes("restoreInstance"), "Deep stack should not contain restoreInstance: " + line);
    }

    // Because of tail calls, the stack should NOT grow with depth — we should see
    // roughly the same number of wasm frames regardless of depth.
    let wasmFrameCount = lines.filter(l => l.includes("wasm-function")).length;
    assert.truthy(wasmFrameCount <= 4, "Tail calls should collapse frames, got " + wasmFrameCount);
}

await assert.asyncTest(test())
