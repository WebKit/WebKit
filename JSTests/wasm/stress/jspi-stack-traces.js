//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that JSPI internal wrapper functions are invisible in stack traces.
// Specifically, no stack frame should mention "promising", "Suspending", or
// "pinball" (the internal fulfill handler used for resuspension). The stack
// should go directly from the JS caller to the wasm functions to the JS
// callee.
//
// Two wasm modules are used:
//
//   wasmModule:  JS -> promising(wasmEntry) -> callImport -> Suspending(js_func) -> JS
//   deepModule:  JS -> promising(threeTimes) -> wrappedWasm -> Suspending(js_func) -> JS
//                (threeTimes calls wrappedWasm three times and sums the results)
//
// Six test cases cover the combinations of:
//   - Sync vs. async (suspended) stack capture
//   - Normal return vs. exception
//   - Single-call vs. multi-call (deep) wasm chains

let wasmModule = `
(module
  (import "env" "js_func" (func $js_func (result i32)))
  (func $callImport (export "callImport") (result i32)
    call $js_func
  )
  (func $wasmEntry (export "wasmEntry") (result i32)
    call $callImport
  )
)`;

// Test 1: Stack trace captured synchronously (before any suspension)
// The JS callback returns a plain value, no suspension occurs.

async function testSyncStackTrace() {
    let capturedStack = null;

    function captureStack() {
        capturedStack = new Error().stack;
        return 42;
    }

    const instance = await instantiate(wasmModule, {
        env: {
            js_func: new WebAssembly.Suspending(captureStack)
        }
    });
    const entry = WebAssembly.promising(instance.exports.wasmEntry);

    const result = await entry();
    assert.eq(result, 42);
    assert.truthy(capturedStack !== null, "Stack should have been captured");

    const frames = capturedStack.split("\n").map(f => f.trim()).filter(f => f.length > 0);

    // Check that no frame mentions JSPI internal functions
    for (const frame of frames) {
        assert.falsy(frame.includes("promising"), "No frame should contain 'promising', got: " + frame);
        assert.falsy(frame.includes("Suspending"), "No frame should contain 'Suspending', got: " + frame);
        assert.falsy(frame.includes("pinball"), "No frame should contain 'pinball', got: " + frame);
    }

    // Verify the expected frame structure: captureStack -> callImport (wasm) -> wasmEntry (wasm)
    let captureIdx = frames.findIndex(f => f.startsWith("captureStack@"));
    assert.truthy(captureIdx >= 0, "Should find captureStack frame in: " + capturedStack);
    assert.truthy(frames[captureIdx + 1].includes("wasm-function"),
        "Frame after captureStack should be wasm callImport, got: " + frames[captureIdx + 1]);
    assert.truthy(frames[captureIdx + 2].includes("wasm-function"),
        "Frame after callImport should be wasm wasmEntry, got: " + frames[captureIdx + 2]);
}

// Test 2: Stack trace captured after suspension (the JS callback is async and awaits)
// This triggers an actual JSPI suspension and resumption.

async function testAsyncStackTrace() {
    let capturedStack = null;

    async function captureStackAsync() {
        await Promise.resolve(); // Force suspension
        capturedStack = new Error().stack;
        return 99;
    }

    const instance = await instantiate(wasmModule, {
        env: {
            js_func: new WebAssembly.Suspending(captureStackAsync)
        }
    });
    const entry = WebAssembly.promising(instance.exports.wasmEntry);

    const result = await entry();
    assert.eq(result, 99);
    assert.truthy(capturedStack !== null, "Stack should have been captured after resume");

    const frames = capturedStack.split("\n").map(f => f.trim()).filter(f => f.length > 0);

    // After suspension and resumption, the stack may be shorter (only the async
    // continuation), but it should still not contain promising/Suspending frames.
    for (const frame of frames) {
        assert.falsy(frame.includes("promising"), "No frame should contain 'promising' after resume, got: " + frame);
        assert.falsy(frame.includes("Suspending"), "No frame should contain 'Suspending' after resume, got: " + frame);
        assert.falsy(frame.includes("pinball"), "No frame should contain 'pinball' after resume, got: " + frame);
    }
}

// Test 3: Stack trace from an exception thrown before suspension.
// Verify the stack in the caught rejection doesn't contain JSPI wrapper frames.

async function testExceptionStackTrace() {
    function throwFromJS() {
        throw new Error("intentional");
    }

    const instance = await instantiate(wasmModule, {
        env: {
            js_func: new WebAssembly.Suspending(throwFromJS)
        }
    });
    const entry = WebAssembly.promising(instance.exports.wasmEntry);

    try {
        await entry();
        assert.truthy(false, "Should have thrown");
    } catch (e) {
        assert.truthy(e instanceof Error);
        assert.eq(e.message, "intentional");
        const frames = e.stack.split("\n").map(f => f.trim()).filter(f => f.length > 0);

        // The throw site should be visible
        let throwIdx = frames.findIndex(f => f.startsWith("throwFromJS@"));
        assert.truthy(throwIdx >= 0, "Should find throwFromJS in stack: " + e.stack);

        // No JSPI wrapper frames
        for (const frame of frames) {
            assert.falsy(frame.includes("promising"), "Exception stack should not contain 'promising', got: " + frame);
            assert.falsy(frame.includes("Suspending"), "Exception stack should not contain 'Suspending', got: " + frame);
            assert.falsy(frame.includes("pinball"), "Exception stack should not contain 'pinball', got: " + frame);
        }
    }
}

// Test 4: Stack trace from an exception thrown after suspension (async throw).

async function testAsyncExceptionStackTrace() {
    async function throwAfterSuspend() {
        await Promise.resolve();
        throw new Error("async intentional");
    }

    const instance = await instantiate(wasmModule, {
        env: {
            js_func: new WebAssembly.Suspending(throwAfterSuspend)
        }
    });
    const entry = WebAssembly.promising(instance.exports.wasmEntry);

    try {
        await entry();
        assert.truthy(false, "Should have thrown");
    } catch (e) {
        assert.truthy(e instanceof Error);
        assert.eq(e.message, "async intentional");
        const frames = e.stack.split("\n").map(f => f.trim()).filter(f => f.length > 0);

        for (const frame of frames) {
            assert.falsy(frame.includes("promising"), "Async exception stack should not contain 'promising', got: " + frame);
            assert.falsy(frame.includes("Suspending"), "Async exception stack should not contain 'Suspending', got: " + frame);
            assert.falsy(frame.includes("pinball"), "Async exception stack should not contain 'pinball', got: " + frame);
        }
    }
}

// Test 5: Deeper wasm call chain with multiple calls through the Suspending import.
// Wasm function "threeTimes" calls the import three times and sums the results.
// This matches the V8 test structure more closely.

let deepModule = `
(module
  (import "env" "js_func" (func $js_func (result i32)))
  (func $wrappedWasm (export "wrappedWasm") (result i32)
    call $js_func
  )
  (func $threeTimes (export "threeTimes") (result i32)
    call $wrappedWasm
    call $wrappedWasm
    i32.add
    call $wrappedWasm
    i32.add
  )
)`;

async function testDeepCallChain() {
    let stacks = [];

    function captureAndReturn() {
        stacks.push(new Error().stack);
        return 10;
    }

    const instance = await instantiate(deepModule, {
        env: {
            js_func: new WebAssembly.Suspending(captureAndReturn)
        }
    });
    const entry = WebAssembly.promising(instance.exports.threeTimes);

    const result = await entry();
    assert.eq(result, 30); // 10 + 10 + 10
    assert.eq(stacks.length, 3);

    // Check each captured stack
    for (let i = 0; i < stacks.length; i++) {
        const frames = stacks[i].split("\n").map(f => f.trim()).filter(f => f.length > 0);

        // Should contain captureAndReturn, wrappedWasm, threeTimes, but not promising/Suspending
        let captureIdx = frames.findIndex(f => f.startsWith("captureAndReturn@"));
        assert.truthy(captureIdx >= 0, `Stack ${i}: should find captureAndReturn frame in: ` + stacks[i]);

        for (const frame of frames) {
            assert.falsy(frame.includes("promising"), `Stack ${i}: should not contain 'promising', got: ` + frame);
            assert.falsy(frame.includes("Suspending"), `Stack ${i}: should not contain 'Suspending', got: ` + frame);
            assert.falsy(frame.includes("pinball"), `Stack ${i}: should not contain 'pinball', got: ` + frame);
        }
    }
}

// Test 6: Deep call chain with suspension (async import that awaits).

async function testDeepCallChainWithSuspension() {
    let stacks = [];
    let callCount = 0;

    async function captureAndReturnAsync() {
        callCount++;
        await Promise.resolve(); // Force suspension
        stacks.push(new Error().stack);
        return 10;
    }

    const instance = await instantiate(deepModule, {
        env: {
            js_func: new WebAssembly.Suspending(captureAndReturnAsync)
        }
    });
    const entry = WebAssembly.promising(instance.exports.threeTimes);

    const result = await entry();
    assert.eq(result, 30);
    assert.eq(callCount, 3);
    assert.eq(stacks.length, 3);

    for (let i = 0; i < stacks.length; i++) {
        const frames = stacks[i].split("\n").map(f => f.trim()).filter(f => f.length > 0);

        for (const frame of frames) {
            assert.falsy(frame.includes("promising"), `Async stack ${i}: should not contain 'promising', got: ` + frame);
            assert.falsy(frame.includes("Suspending"), `Async stack ${i}: should not contain 'Suspending', got: ` + frame);
            assert.falsy(frame.includes("pinball"), `Async stack ${i}: should not contain 'pinball', got: ` + frame);
        }
    }
}

await testSyncStackTrace();
await testAsyncStackTrace();
await testExceptionStackTrace();
await testAsyncExceptionStackTrace();
await testDeepCallChain();
await testDeepCallChainWithSuspension();
