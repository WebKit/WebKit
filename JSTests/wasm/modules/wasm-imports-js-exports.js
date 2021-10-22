import { addOne } from "./wasm-imports-js-exports/imports.wasm"
import * as assert from '../assert.js';

assert.isFunction(addOne);
assert.eq(addOne(32), 33);
assert.eq(addOne(-2), -1);
assert.eq(addOne(0x7fffffff), -2147483648);

import { incrementCount } from "./wasm-imports-js-exports/global.wasm";
import { count } from "./wasm-imports-js-exports/global.js";

assert.isFunction(incrementCount);
assert.eq(count.valueOf(), 42);
incrementCount();
assert.eq(count.valueOf(), 43);

import { getElem } from "./wasm-imports-js-exports/table.wasm";

assert.isFunction(getElem);
assert.eq(getElem(), "foo");
