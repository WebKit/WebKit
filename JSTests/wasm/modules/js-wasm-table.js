import { table } from "./table.wasm"
import * as assert from '../assert.js';

assert.instanceof(table, WebAssembly.Table);
assert.eq(table.length, 3);
assert.isFunction(table.get(0));
assert.isFunction(table.get(1));
assert.eq(table.get(2), null);

assert.eq(table.get(0)(), 42);
assert.eq(table.get(1)(), 83);

assert.throws(() => {
    table = 32;
}, TypeError, `Attempted to assign to readonly property.`);
