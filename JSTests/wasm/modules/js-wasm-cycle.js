import * as assert from '../assert.js';
import("./js-wasm-cycle/entry.js").then($vm.abort, function (error) {
    assert.eq(String(error), `Error: import function ./entry.js:return42 must be callable`);
}).then(function () { }, $vm.abort);
