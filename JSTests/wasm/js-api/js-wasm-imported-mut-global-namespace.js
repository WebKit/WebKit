import * as ns from "./reexport-js-mut-global.wasm"
import { g } from "./js-module-mutable-global-export.js"
import * as assert from "../assert.js";

assert.eq(ns.g, 1);
assert.instanceof(g, WebAssembly.Global);
assert.eq(g.value, 1);

g.value = 9;
assert.eq(ns.g, 9);
assert.eq(g.value, 9);
