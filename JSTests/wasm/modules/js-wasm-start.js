import * as assert from '../assert.js';

Promise.all([
    import("./start.wasm"),
    import("./start.wasm"),
]).then(([start0, start1]) => {
    assert.eq(start0, start1);
    assert.eq(start0.get(), 1);
}, $vm.abort);
