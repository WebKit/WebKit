import * as constant from "./constant.wasm"
import * as assert from '../assert.js';

assert.isNumber(constant.constant.value);
assert.eq(constant.constant.value, 42);
assert.throws(() => {
    constant.constant.value = 200;
}, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
