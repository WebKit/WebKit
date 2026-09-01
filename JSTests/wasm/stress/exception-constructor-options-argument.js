//@ requireOptions("--useExecutableAllocationFuzz=false")
import * as assert from "../assert.js";

const tag = new WebAssembly.Tag({ parameters: ["i32"] });

// The options argument is a WebIDL dictionary: undefined and null mean an empty
// dictionary, and anything else that is not an object is a TypeError.

function testUndefinedAndNull() {
    for (const options of [undefined, null]) {
        const e = new WebAssembly.Exception(tag, [0], options);
        assert.truthy(e instanceof WebAssembly.Exception);
        assert.eq(e.stack, undefined);
    }
}

function testNonObjects() {
    for (const options of [5, 0, "traceStack", "", true, false, 1n, Symbol("s")]) {
        assert.throws(() => new WebAssembly.Exception(tag, [0], options), TypeError,
            "WebAssembly.Exception expects its third argument to be an object");
    }
}

function testObjects() {
    for (const options of [{}, [], () => {}, Object.create(null), { traceStack: false }]) {
        const e = new WebAssembly.Exception(tag, [0], options);
        assert.eq(e.stack, undefined);
    }
    for (const options of [{ traceStack: true }, { traceStack: 1 }, { traceStack: "yes" }, Object.assign(() => {}, { traceStack: true })]) {
        const e = new WebAssembly.Exception(tag, [0], options);
        assert.eq(typeof e.stack, "string");
    }
}

testUndefinedAndNull();
testNonObjects();
testObjects();
