import * as ns from "./mut-global.wasm"
import * as assert from '../assert.js';

assert.eq(ns.g, 100);
assert.eq(ns.get(), 100);

ns.set(555);
assert.eq(ns.get(), 555);
assert.eq(ns.g, 555);

assert.throws(() => {
    ns.g = 1;
}, TypeError, `Attempted to assign to readonly property.`);
