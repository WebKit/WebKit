import { g, get, set } from "./js-reexport-wasm-mut-global.js"
import * as assert from "../assert.js";

assert.eq(g, 100);
set(3);
assert.eq(get(), 3);
assert.eq(g, 3);
