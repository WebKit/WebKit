import * as assert from '../assert.js';
import("./wasm-wasm-cycle/entry.wasm").then($vm.abort, function (error) {
    assert.eq(String(error), `Error: import function ./entry.wasm:return42 must be callable`);
}).then(function () { }, $vm.abort);
