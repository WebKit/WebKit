import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Test constant folding of f32/f64 min/max with edge cases that the spec tests
// don't cover (spec tests pass values as parameters, never as inline constants).

let wat = `
(module
    ;; Signed zeros
    (func (export "f32_min_pos_neg_zero") (result f32) f32.const 0.0   f32.const -0.0  f32.min)
    (func (export "f32_min_neg_pos_zero") (result f32) f32.const -0.0  f32.const 0.0   f32.min)
    (func (export "f32_max_pos_neg_zero") (result f32) f32.const 0.0   f32.const -0.0  f32.max)
    (func (export "f32_max_neg_pos_zero") (result f32) f32.const -0.0  f32.const 0.0   f32.max)
    (func (export "f64_min_pos_neg_zero") (result f64) f64.const 0.0   f64.const -0.0  f64.min)
    (func (export "f64_min_neg_pos_zero") (result f64) f64.const -0.0  f64.const 0.0   f64.min)
    (func (export "f64_max_pos_neg_zero") (result f64) f64.const 0.0   f64.const -0.0  f64.max)
    (func (export "f64_max_neg_pos_zero") (result f64) f64.const -0.0  f64.const 0.0   f64.max)

    ;; NaN propagation
    (func (export "f32_min_nan_one")  (result f32) f32.const nan    f32.const 1.0  f32.min)
    (func (export "f32_min_one_nan")  (result f32) f32.const 1.0    f32.const nan  f32.min)
    (func (export "f32_max_nan_one")  (result f32) f32.const nan    f32.const 1.0  f32.max)
    (func (export "f32_max_one_nan")  (result f32) f32.const 1.0    f32.const nan  f32.max)
    (func (export "f64_min_nan_one")  (result f64) f64.const nan    f64.const 1.0  f64.min)
    (func (export "f64_min_one_nan")  (result f64) f64.const 1.0    f64.const nan  f64.min)
    (func (export "f64_max_nan_one")  (result f64) f64.const nan    f64.const 1.0  f64.max)
    (func (export "f64_max_one_nan")  (result f64) f64.const 1.0    f64.const nan  f64.max)

    ;; Both NaN
    (func (export "f32_min_nan_nan") (result f32) f32.const nan f32.const nan f32.min)
    (func (export "f32_max_nan_nan") (result f32) f32.const nan f32.const nan f32.max)
    (func (export "f64_min_nan_nan") (result f64) f64.const nan f64.const nan f64.min)
    (func (export "f64_max_nan_nan") (result f64) f64.const nan f64.const nan f64.max)

    ;; Denormals (smallest subnormal)
    (func (export "f32_min_denorm") (result f32) f32.const 0x1p-149 f32.const -0x1p-149 f32.min)
    (func (export "f32_max_denorm") (result f32) f32.const -0x1p-149 f32.const 0x1p-149 f32.max)
    (func (export "f64_min_denorm") (result f64) f64.const 0x1p-1074 f64.const -0x1p-1074 f64.min)
    (func (export "f64_max_denorm") (result f64) f64.const -0x1p-1074 f64.const 0x1p-1074 f64.max)

    ;; Infinity
    (func (export "f32_min_inf")     (result f32) f32.const inf  f32.const -inf f32.min)
    (func (export "f32_max_inf")     (result f32) f32.const -inf f32.const inf  f32.max)
    (func (export "f32_min_inf_one") (result f32) f32.const inf  f32.const 1.0  f32.min)
    (func (export "f32_max_inf_one") (result f32) f32.const -inf f32.const 1.0  f32.max)
    (func (export "f64_min_inf")     (result f64) f64.const inf  f64.const -inf f64.min)
    (func (export "f64_max_inf")     (result f64) f64.const -inf f64.const inf  f64.max)

    ;; NaN with infinity
    (func (export "f32_min_nan_inf") (result f32) f32.const nan f32.const inf f32.min)
    (func (export "f32_max_nan_inf") (result f32) f32.const nan f32.const -inf f32.max)
    (func (export "f64_min_nan_inf") (result f64) f64.const nan f64.const inf f64.min)
    (func (export "f64_max_nan_inf") (result f64) f64.const nan f64.const -inf f64.max)

    ;; Normal values
    (func (export "f32_min_normal") (result f32) f32.const 3.14 f32.const 2.72 f32.min)
    (func (export "f32_max_normal") (result f32) f32.const 3.14 f32.const 2.72 f32.max)
    (func (export "f64_min_normal") (result f64) f64.const 3.14 f64.const 2.72 f64.min)
    (func (export "f64_max_normal") (result f64) f64.const 3.14 f64.const 2.72 f64.max)
)
`;

let instance = await instantiate(wat);

for (let i = 0; i < wasmTestLoopCount; ++i) {
    // Signed zeros: min should return -0, max should return +0
    assert.eq(instance.exports.f32_min_pos_neg_zero(), -0.0);
    assert.eq(instance.exports.f32_min_neg_pos_zero(), -0.0);
    assert.eq(instance.exports.f32_max_pos_neg_zero(), 0.0);
    assert.eq(instance.exports.f32_max_neg_pos_zero(), 0.0);
    assert.eq(instance.exports.f64_min_pos_neg_zero(), -0.0);
    assert.eq(instance.exports.f64_min_neg_pos_zero(), -0.0);
    assert.eq(instance.exports.f64_max_pos_neg_zero(), 0.0);
    assert.eq(instance.exports.f64_max_neg_pos_zero(), 0.0);

    // NaN propagation: any NaN operand produces NaN
    assert.eq(instance.exports.f32_min_nan_one(), NaN);
    assert.eq(instance.exports.f32_min_one_nan(), NaN);
    assert.eq(instance.exports.f32_max_nan_one(), NaN);
    assert.eq(instance.exports.f32_max_one_nan(), NaN);
    assert.eq(instance.exports.f64_min_nan_one(), NaN);
    assert.eq(instance.exports.f64_min_one_nan(), NaN);
    assert.eq(instance.exports.f64_max_nan_one(), NaN);
    assert.eq(instance.exports.f64_max_one_nan(), NaN);

    // Both NaN
    assert.eq(instance.exports.f32_min_nan_nan(), NaN);
    assert.eq(instance.exports.f32_max_nan_nan(), NaN);
    assert.eq(instance.exports.f64_min_nan_nan(), NaN);
    assert.eq(instance.exports.f64_max_nan_nan(), NaN);

    // Denormals
    assert.eq(instance.exports.f32_min_denorm(), -Math.fround(Math.pow(2, -149)));
    assert.eq(instance.exports.f32_max_denorm(), Math.fround(Math.pow(2, -149)));
    assert.eq(instance.exports.f64_min_denorm(), -Math.pow(2, -1074));
    assert.eq(instance.exports.f64_max_denorm(), Math.pow(2, -1074));

    // Infinity
    assert.eq(instance.exports.f32_min_inf(), -Infinity);
    assert.eq(instance.exports.f32_max_inf(), Infinity);
    assert.eq(instance.exports.f32_min_inf_one(), 1.0);
    assert.eq(instance.exports.f32_max_inf_one(), 1.0);
    assert.eq(instance.exports.f64_min_inf(), -Infinity);
    assert.eq(instance.exports.f64_max_inf(), Infinity);

    // NaN with infinity
    assert.eq(instance.exports.f32_min_nan_inf(), NaN);
    assert.eq(instance.exports.f32_max_nan_inf(), NaN);
    assert.eq(instance.exports.f64_min_nan_inf(), NaN);
    assert.eq(instance.exports.f64_max_nan_inf(), NaN);

    // Normal values
    assert.eq(instance.exports.f32_min_normal(), Math.fround(2.72));
    assert.eq(instance.exports.f32_max_normal(), Math.fround(3.14));
    assert.eq(instance.exports.f64_min_normal(), 2.72);
    assert.eq(instance.exports.f64_max_normal(), 3.14);
}
