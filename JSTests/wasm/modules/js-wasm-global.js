import { constant } from "./constant.wasm"
import * as assert from '../assert.js';

assert.isNumber(constant.value);
assert.eq(constant.value, 42);
assert.throws(() => {
    constant.value = 200;
}, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
