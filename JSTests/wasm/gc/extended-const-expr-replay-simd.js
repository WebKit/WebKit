//@ skip unless $isSIMDPlatform
//@ requireOptions("--useWasmSIMD=1")

import * as assert from "../assert.js";
import { compile } from "./wast-wrapper.js";

// v128.const only reaches the constant expression evaluator as an operand of a larger expression,
// so it has to be tested inside one rather than as a lone initializer.
const module = compile(`(module
  (type $vectors (array (mut v128)))
  (global $vectors (ref $vectors)
    (array.new_fixed $vectors 2 (v128.const i32x4 1 2 3 4) (v128.const i32x4 5 6 7 8)))
  (func (export "lane") (param i32) (result i32)
    (i32x4.extract_lane 0 (array.get $vectors (global.get $vectors) (local.get 0)))))`);

const first = new WebAssembly.Instance(module);
assert.eq(first.exports.lane(0), 1);
assert.eq(first.exports.lane(1), 5);

// The expression is evaluated again for the second instance, from the same bytes.
const second = new WebAssembly.Instance(module);
assert.eq(second.exports.lane(0), 1);
assert.eq(second.exports.lane(1), 5);
