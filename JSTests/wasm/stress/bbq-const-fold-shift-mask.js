import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// https://bugs.webkit.org/show_bug.cgi?id=270257
// BBQ const-fold of shifts must mask the count (i32 & 31, i64 & 63).

let wat = `
(module
  (func (export "i32_shr_u") (result i32)
    i32.const 2147483647
    i32.const 536870909
    i32.shr_u)
  (func (export "i64_shr_u") (result i64)
    i64.const 0x7fffffffffffffff
    i64.const 0x1ffffffd
    i64.shr_u)
)
`;

let instance = await instantiate(wat);
// 536870909 & 31 == 29; 0x7fffffff >>> 29 == 3
assert.eq(instance.exports.i32_shr_u(), 3);
// 0x1ffffffd & 63 == 61; 0x7fffffffffffffff >>> 61 == 3
assert.eq(instance.exports.i64_shr_u(), 3n);
