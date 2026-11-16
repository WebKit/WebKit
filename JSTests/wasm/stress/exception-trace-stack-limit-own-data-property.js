//@ requireOptions("--useExecutableAllocationFuzz=false")
import * as assert from "../assert.js";

function frameCount(WebAssembly, tag, depth) {
    function recurse(x) {
        if (x) {
            const exception = recurse(x - 1);
            return exception;
        }
        return new WebAssembly.Exception(tag, [], { traceStack: true });
    }
    const stack = recurse(depth).stack;
    assert.eq(typeof stack, "string");
    return stack === "" ? 0 : stack.split("\n").length;
}

const defaultLimit = Error.stackTraceLimit;
const tag = new WebAssembly.Tag({ parameters: [] });

const other = createGlobalObject();
const otherTag = new other.WebAssembly.Tag({ parameters: [] });
assert.eq(frameCount(other.WebAssembly, otherTag, defaultLimit + 50), defaultLimit);
other.Error.stackTraceLimit = 3;
assert.eq(frameCount(other.WebAssembly, otherTag, 20), 3);
assert.eq(frameCount(WebAssembly, tag, defaultLimit + 50), defaultLimit);

Error.stackTraceLimit = 10;
assert.eq(frameCount(WebAssembly, tag, 20), 10);

Object.defineProperty(Error, "stackTraceLimit", { value: 4 });
assert.eq(frameCount(WebAssembly, tag, 20), 4);

let getterCalls = 0;
Object.defineProperty(Error, "stackTraceLimit", {
    get() {
        getterCalls++;
        return 6;
    },
    set(value) { },
});
assert.eq(frameCount(WebAssembly, tag, 20), 0);
assert.eq(getterCalls, 0);

delete Error.stackTraceLimit;
assert.eq(frameCount(WebAssembly, tag, 20), 0);

Error.stackTraceLimit = 5;
assert.eq(frameCount(WebAssembly, tag, 20), 5);
