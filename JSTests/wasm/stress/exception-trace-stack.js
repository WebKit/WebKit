//@ requireOptions("--useExecutableAllocationFuzz=false")
import * as assert from "../assert.js";

const tag = new WebAssembly.Tag({ parameters: ["i32"] });

function testDefaultHasNoStack() {
    const e = new WebAssembly.Exception(tag, [0]);
    assert.eq(e.stack, undefined);
}

function testTraceStackFalse() {
    const e = new WebAssembly.Exception(tag, [0], { traceStack: false });
    assert.eq(e.stack, undefined);
}

function testTraceStackTrue() {
    function makeException() {
        return new WebAssembly.Exception(tag, [0], { traceStack: true });
    }
    const e = makeException();
    assert.eq(typeof e.stack, "string");
    assert.truthy(e.stack.length > 0, "stack should be non-empty");
    assert.truthy(e.stack.includes("makeException") || e.stack.includes("testTraceStackTrue"),
        `stack should include a JS frame, got:\n${e.stack}`);
    // IDL: stack is a prototype getter, not an own data property.
    assert.eq(Object.hasOwn(e, "stack"), false);
    const desc = Object.getOwnPropertyDescriptor(WebAssembly.Exception.prototype, "stack");
    assert.eq(typeof desc.get, "function");
}

function testConstructorLength() {
    assert.eq(WebAssembly.Exception.length, 2);
}

testDefaultHasNoStack();
testTraceStackFalse();
testTraceStackTrue();
testConstructorLength();
print("exception-trace-stack: ok");
