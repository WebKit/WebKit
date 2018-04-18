import * as assert from '../assert.js';
import("./wasm-imports-wasm-exports-i64-error/imports.wasm").then($vm.abort, function (error) {
    assert.eq(String(error), `Error: exported global cannot be an i64`);
}).catch($vm.abort);
