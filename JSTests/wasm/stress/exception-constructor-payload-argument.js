import * as assert from "../assert.js";

const tag = new WebAssembly.Tag({ parameters: ["i32"] });
const empty = new WebAssembly.Tag({ parameters: [] });

for (const payload of ["a", "", 1, 0, true, false, null, undefined, 1n, Symbol("s")]) {
    assert.throws(() => new WebAssembly.Exception(tag, payload), TypeError, "WebAssembly.Exception constructor expects the payload argument to be an object");
    assert.throws(() => new WebAssembly.Exception(empty, payload), TypeError, "WebAssembly.Exception constructor expects the payload argument to be an object");
}

const exception = new WebAssembly.Exception(tag, [1]);
assert.truthy(exception instanceof WebAssembly.Exception);
assert.truthy(new WebAssembly.Exception(empty, []) instanceof WebAssembly.Exception);
