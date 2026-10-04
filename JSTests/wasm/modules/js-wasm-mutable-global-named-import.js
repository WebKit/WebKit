import { g, get, set } from "./mut-global.wasm"
import { g as renamed } from "./mut-global.wasm"
import * as assert from '../assert.js';

assert.eq(g, 100);
assert.eq(renamed, 100);
assert.eq(get(), 100);

set(555);
assert.eq(get(), 555);
assert.eq(g, 555);
assert.eq(renamed, 555);

set(0);
assert.eq(g, 0);
assert.eq(renamed, 0);
