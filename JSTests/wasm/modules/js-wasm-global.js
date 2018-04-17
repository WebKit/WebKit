import { constant } from "./constant.wasm"
import * as assert from '../assert.js';

assert.isNumber(constant);
assert.eq(constant, 42);
assert.throws(() => {
    constant = 200;
}, TypeError, `Attempted to assign to readonly property.`);
