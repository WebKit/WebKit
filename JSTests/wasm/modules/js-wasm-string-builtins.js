import * as ns from "./js-string-builtins.wasm"
import * as assert from "../assert.js";

assert.eq(ns.getLength("hello"), 5);
assert.eq(ns.concatStrings("hello", " world"), "hello world");
assert.eq(ns.compareStrings("test", "test"), 1);
assert.eq(ns.compareStrings("test", "different"), 0);
assert.eq(ns.testString("hello"), 1);
assert.eq(ns.testString(42), 0);
