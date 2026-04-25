import * as sum from "./sum.wasm"
import * as assert from '../assert.js';

assert.isObject(sum);
assert.isFunction(sum.sum);
assert.eq(sum.sum(32, 42), 74);
assert.eq(sum.sum(-2, 42), 40);

assert.throws(() => {
    sum.sum = 32;
}, TypeError, `Attempted to assign to readonly property.`);
