//@ skip if $architecture != "arm64"
//@ $skipModes << "wasm-bbq".to_sym
//@ $skipModes << "wasm-bbq-no-consts".to_sym
//@ requireOptions("--useExecutableAllocationFuzz=false", "--enableWasmDebugger=true", "--useBBQJIT=0", "--useOMGJIT=0")
import * as assert from "../assert.js";

// Bug 278991 / 318712: with enableWasmDebugger (IPInt only), Error.stack includes :0xOFF.

function moduleUnreachable() {
    // (module (func (export "run") unreachable))
    // FunctionData.start = 0x1f; unreachable at module offset 0x20.
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x00,
        0x0a, 0x05, 0x01, 0x03, 0x00, 0x00, 0x0b,
    ]);
}

function moduleDivZero() {
    // (module (func (export "run") i32.const 1 i32.const 0 i32.div_s drop))
    // FunctionData.start = 0x1f; i32.div_s at module offset 0x24.
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x00,
        0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x01, 0x41, 0x00, 0x6d, 0x1a, 0x0b,
    ]);
}

function moduleImportCall() {
    // (module (import "e" "f" (func)) (func (export "run") (call 0)))
    // Defined function body starts at 0x28; call at 0x29.
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x02, 0x07, 0x01, 0x01, 0x65, 0x01, 0x66, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01,
        0x0a, 0x06, 0x01, 0x04, 0x00, 0x10, 0x00, 0x0b,
    ]);
}

function moduleOOBLoad() {
    // (module (memory 0) (func (export "run") i32.const 0 i32.load drop))
    // FunctionData.start = 0x24; i32.load at module offset 0x27.
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x03, 0x02, 0x01, 0x00,
        0x05, 0x03, 0x01, 0x00, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x00,
        0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x1a, 0x0b,
    ]);
}

// Google Maps toy from https://bugs.webkit.org/show_bug.cgi?id=278991#c4
// Chrome reports wasm-function[0]:0x34 (i32.load).
function moduleMapsOOB() {
    const s = "AGFzbQEAAAABBgFgAX8BfwMCAQAFAwEAAQcSAgZtZW1vcnkCAAVkZXJlZgAACgkBBwAgACgCAAsAFQRuYW1lAgMBAAAGCQEABm1lbW9yeQ";
    const pad = s + "=".repeat((4 - s.length % 4) % 4);
    return Uint8Array.from(atob(pad), (c) => c.charCodeAt(0));
}

function firstWasmFrame(stack) {
    const line = stack.split("\n").find((l) => l.includes("wasm-function["));
    assert.truthy(line, `expected wasm frame in stack:\n${stack}`);
    return line;
}

function assertFrameOffset(stack, index, offset) {
    const line = firstWasmFrame(stack);
    const expected = `wasm-function[${index}]:0x${offset.toString(16)}`;
    assert.truthy(line.includes(expected), `expected ${expected} in:\n${stack}`);
}

function testUnreachableOffset() {
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleUnreachable()));
    let stack = null;
    try {
        inst.exports.run();
    } catch (e) {
        stack = e.stack;
    }
    assert.truthy(stack, "should throw");
    assertFrameOffset(stack, 0, 0x20);
}

function testDivZeroOffset() {
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleDivZero()));
    let stack = null;
    try {
        inst.exports.run();
    } catch (e) {
        stack = e.stack;
    }
    assert.truthy(stack, "should throw");
    assertFrameOffset(stack, 0, 0x24);
}

function testImportCallOffset() {
    let stack = null;
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleImportCall()), {
        e: {
            f() {
                stack = new Error().stack;
            }
        }
    });
    inst.exports.run();
    assert.truthy(stack, "callback should run");
    assertFrameOffset(stack, 1, 0x29);
}

function testOOBLoadOffset() {
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleOOBLoad()));
    let stack = null;
    try {
        inst.exports.run();
    } catch (e) {
        stack = e.stack;
    }
    assert.truthy(stack, "should throw");
    assertFrameOffset(stack, 0, 0x27);
}

function testMapsOOBOffset() {
    const inst = new WebAssembly.Instance(new WebAssembly.Module(moduleMapsOOB()));
    let stack = null;
    try {
        inst.exports.deref(-1);
    } catch (e) {
        stack = e.stack;
    }
    assert.truthy(stack, "should throw");
    assertFrameOffset(stack, 0, 0x34);
}

testUnreachableOffset();
testDivZeroOffset();
testImportCallOffset();
testOOBLoadOffset();
testMapsOOBOffset();
