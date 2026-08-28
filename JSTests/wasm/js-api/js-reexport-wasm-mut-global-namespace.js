import * as ns from "./js-reexport-wasm-mut-global.js"
import * as assert from "../assert.js";

assert.eq(ns.g, 100);
ns.set(3);
assert.eq(ns.get(), 3);
assert.eq(ns.g, 3);
