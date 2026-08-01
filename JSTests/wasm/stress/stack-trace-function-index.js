//@ skip if $architecture != "arm64"
//@ requireOptions("--useExecutableAllocationFuzz=false", "--enableWasmDebugger=true")
import * as assert from "../assert.js";

// Bug 278991 / 318712: with enableWasmDebugger, Error.stack uses wasm-function[index]
// (debugger forces IPInt so stacks are tier-stable).

function moduleUnreachable() {
    // (module (func (export "run") unreachable))
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x00,
        0x0a, 0x05, 0x01, 0x03, 0x00, 0x00, 0x0b,
    ]);
}

function moduleWithImportCallback() {
    // (module
    //   (import "env" "cb" (func $cb))
    //   (func (export "run") (call $cb)))
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x02, 0x0a, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x02, 0x63, 0x62, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01,
        0x0a, 0x06, 0x01, 0x04, 0x00, 0x10, 0x00, 0x0b,
    ]);
}

function assertHasWasmFunctionIndex(stack, index) {
    const needle = `wasm-function[${index}]`;
    assert.truthy(stack.includes(needle), `stack should contain ${needle}, got:\n${stack}`);
    // Production format is `N@wasm-function[N]`; debugger mode puts the index only in the location.
    const productionStyle = new RegExp(String.raw`^\d+@wasm-function\[${index}\]`, "m");
    assert.falsy(productionStyle.test(stack), `debugger mode should not use production N@wasm-function form, got:\n${stack}`);
}

function testUnreachableHasIndex() {
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleUnreachable()));
    let stack = null;
    try {
        inst.exports.run();
    } catch (e) {
        stack = e.stack;
    }
    assert.truthy(stack, "should throw");
    assertHasWasmFunctionIndex(stack, 0);
}

function testImportCallbackSeesCallerIndex() {
    let stack = null;
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleWithImportCallback()), {
        env: {
            cb() {
                stack = new Error().stack;
            }
        }
    });
    inst.exports.run();
    assert.truthy(stack, "callback should run");
    // Import is index 0; defined function is index 1.
    assertHasWasmFunctionIndex(stack, 1);
}

testUnreachableHasIndex();
testImportCallbackSeesCallerIndex();
print("stack-trace-function-index: ok");
