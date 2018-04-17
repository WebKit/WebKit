import { addOne } from "./wasm-imports-js-exports/imports.wasm"
import * as assert from '../assert.js';

assert.isFunction(addOne);
assert.eq(addOne(32), 33);
assert.eq(addOne(-2), -1);
assert.eq(addOne(0x7fffffff), -2147483648);
