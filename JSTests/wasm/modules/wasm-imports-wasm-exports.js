import { addOne, getAnswer, getAnswer1, getAnswer2, getAnswer3, getAnswer4, table } from "./wasm-imports-wasm-exports/imports.wasm"
import { table as table2 } from "./wasm-imports-wasm-exports/sum.wasm"
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

assert.eq(table, table2);
assert.eq(table.length, 4);
assert.eq(table.get(0)(1, 2), 3);
assert.eq(table.get(1)(42), 43);
assert.eq(table.get(2)(), 42);
assert.eq(table.get(3)(), 0.5);
