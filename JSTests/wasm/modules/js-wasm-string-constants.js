import * as ns from "./js-string-constants.wasm"
import * as assert from "../assert.js";

assert.eq(ns.empty, "");
assert.eq(ns.hello, "hello");
assert.eq(ns.emoji, "\u{1F600}");
