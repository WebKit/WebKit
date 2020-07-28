import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const { truncS, truncU } = instantiate(`
(module
 (func (export "truncS") (param f64) (result i32) (local $l0 f64)
   (i32.trunc_f64_s (local.get 0))
 )
 (func (export "truncU") (param f64) (result i32) (local $l0 f64)
   (i32.trunc_f64_u (local.get 0))
 )
)
`).exports;

assert.eq(truncS(-2147483648.1), -2147483648);
assert.eq(truncS(-2147483648.9), -2147483648);
assert.eq(truncS(2147483647.9), 2147483647);
assert.throws(() => truncS(-2147483649), WebAssembly.RuntimeError, `Out of bounds Trunc operation (evaluating 'func(...args)')`);
assert.throws(() => truncS(2147483648), WebAssembly.RuntimeError, `Out of bounds Trunc operation (evaluating 'func(...args)')`);

assert.eq(truncU(-0.9), 0);
assert.eq(truncU(4294967295.9), -1);
assert.throws(() => truncU(-1), WebAssembly.RuntimeError, `Out of bounds Trunc operation (evaluating 'func(...args)')`);
assert.throws(() => truncU(4294967296), WebAssembly.RuntimeError, `Out of bounds Trunc operation (evaluating 'func(...args)')`);
