import * as assert from '../assert.js';

function assertLinkError(promise) {
    return promise.then($vm.abort, function (error) {
        assert.eq(error instanceof WebAssembly.LinkError, true);
    });
}

assertLinkError(import("./reserved-import-name.wasm"))
    .then(() => assertLinkError(import("./reserved-import-name-wasm-js.wasm")))
    .then(() => assertLinkError(import("./reserved-export-name.wasm")))
    .then(() => assertLinkError(import("./reserved-export-name-wasm-js.wasm")))
    .then(() => assertLinkError(import("./reserved-import-module.wasm")))
    .then(() => import("./wasm-colon-module.wasm").then($vm.abort, function (error) {
        assert.eq(error instanceof WebAssembly.LinkError && String(error).includes("is reserved"), false);
    }))
    .then(function () { }, $vm.abort);

const { "wasm:invalid": fn } = new WebAssembly.Instance(new WebAssembly.Module(read("./reserved-export-name.wasm", "binary"))).exports;
assert.isFunction(fn);
assert.eq(fn(), 42);
