import * as assert from '../assert.js';
import("./default-import-star-error/entry.wasm").then($vm.abort, function (error) {
    assert.eq(String(error), `SyntaxError: Importing binding name 'default' cannot be resolved by star export entries.`);
}).then(function () { }, $vm.abort);
