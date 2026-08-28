import * as ns from "./js-module-mutable-global-export.js"
import * as assert from "../assert.js";

assert.instanceof(ns.g, WebAssembly.Global);
assert.eq(ns.g.value, 1);
ns.g.value = 7;
assert.eq(ns.g.value, 7);
assert.instanceof(ns.g, WebAssembly.Global);
