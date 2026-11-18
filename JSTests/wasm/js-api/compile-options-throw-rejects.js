//@ requireOptions("--useWasmJSStringBuiltins=true")
import * as assert from "../assert.js";

const bytes = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);

function rejectionOf(promise) {
    let rejection;
    promise.then(() => { rejection = "fulfilled"; }, error => { rejection = error; });
    drainMicrotasks();
    return rejection;
}

function assertRejected(promise, error) {
    assert.truthy(promise instanceof Promise);
    assert.eq(rejectionOf(promise), error);
}

const compileError = new TypeError("boom");
assertRejected(WebAssembly.compile(bytes, { get builtins() { throw compileError; } }), compileError);

const instantiateError = new TypeError("boom");
assertRejected(WebAssembly.instantiate(bytes, {}, { get builtins() { throw instantiateError; } }), instantiateError);
