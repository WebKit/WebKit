import { addOne, getAnswer, table } from "./wasm-imports-js-re-exports-wasm-exports/imports.wasm"
import * as assert from '../assert.js';

assert.isFunction(addOne);
assert.eq(addOne(32), 33);
assert.eq(addOne(-2), -1);
assert.eq(addOne(0x7fffffff), -2147483648);

assert.eq(getAnswer(), 42);

assert.eq(table.length, 4);
assert.eq(table.get(0)(1, 2), 3);
assert.eq(table.get(1)(-1), 0);
assert.eq(table.get(2), null);
assert.eq(table.get(3), null);
