import { sum } from "./sum.wasm"
import * as assert from '../assert.js';

assert.isFunction(sum);
assert.eq(sum(32, 42), 74);
assert.eq(sum(-2, 42), 40);

assert.throws(() => {
    sum = 32;
}, TypeError, `Attempted to assign to readonly property.`);
