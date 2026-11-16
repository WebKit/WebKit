import * as constant from "./constant.wasm"
import * as assert from '../assert.js';

assert.isNumber(constant.constant);
assert.eq(constant.constant, 42);
assert.throws(() => {
    constant.constant = 200;
}, TypeError, `Attempted to assign to readonly property.`);
