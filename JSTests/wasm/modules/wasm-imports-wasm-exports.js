import { addOne, getAnswer, getAnswer1, getAnswer2, getAnswer3, getAnswer4 } from "./wasm-imports-wasm-exports/imports.wasm"
import * as assert from '../assert.js';

assert.isFunction(addOne);
assert.eq(addOne(32), 33);
assert.eq(addOne(-2), -1);
assert.eq(addOne(0x7fffffff), -2147483648);

assert.eq(getAnswer(), 42);
assert.eq(getAnswer1(), 0.5);
assert.eq(getAnswer2(), 0.5);

assert.truthy(isPureNaN(getAnswer3()));
assert.truthy(isPureNaN(getAnswer4()));
