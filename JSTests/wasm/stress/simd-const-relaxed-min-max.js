//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    ;; f32x4.relaxed_min
    (func (export "test_f32x4_relaxed_min") (result f32)
        (v128.const f32x4 1.0 -2.0 3.0 -4.0)
        (v128.const f32x4 2.0 -1.0 -3.0 4.0)
        (f32x4.relaxed_min)
        (return (f32x4.extract_lane 0))
    )
    (func (export "test_f32x4_relaxed_min_lane1") (result f32)
        (v128.const f32x4 1.0 -2.0 3.0 -4.0)
        (v128.const f32x4 2.0 -1.0 -3.0 4.0)
        (f32x4.relaxed_min)
        (return (f32x4.extract_lane 1))
    )
    ;; f32x4.relaxed_max
    (func (export "test_f32x4_relaxed_max") (result f32)
        (v128.const f32x4 1.0 -2.0 3.0 -4.0)
        (v128.const f32x4 2.0 -1.0 -3.0 4.0)
        (f32x4.relaxed_max)
        (return (f32x4.extract_lane 0))
    )
    (func (export "test_f32x4_relaxed_max_lane3") (result f32)
        (v128.const f32x4 1.0 -2.0 3.0 -4.0)
        (v128.const f32x4 2.0 -1.0 -3.0 4.0)
        (f32x4.relaxed_max)
        (return (f32x4.extract_lane 3))
    )
    ;; f64x2.relaxed_min
    (func (export "test_f64x2_relaxed_min") (result f64)
        (v128.const f64x2 1.5 -3.5)
        (v128.const f64x2 2.5 -2.5)
        (f64x2.relaxed_min)
        (return (f64x2.extract_lane 0))
    )
    (func (export "test_f64x2_relaxed_min_lane1") (result f64)
        (v128.const f64x2 1.5 -3.5)
        (v128.const f64x2 2.5 -2.5)
        (f64x2.relaxed_min)
        (return (f64x2.extract_lane 1))
    )
    ;; f64x2.relaxed_max
    (func (export "test_f64x2_relaxed_max") (result f64)
        (v128.const f64x2 1.5 -3.5)
        (v128.const f64x2 2.5 -2.5)
        (f64x2.relaxed_max)
        (return (f64x2.extract_lane 0))
    )
    (func (export "test_f64x2_relaxed_max_lane1") (result f64)
        (v128.const f64x2 1.5 -3.5)
        (v128.const f64x2 2.5 -2.5)
        (f64x2.relaxed_max)
        (return (f64x2.extract_lane 1))
    )

    ;; Equal values
    (func (export "test_f32x4_relaxed_min_equal") (result f32)
        (v128.const f32x4 5.0 5.0 5.0 5.0)
        (v128.const f32x4 5.0 5.0 5.0 5.0)
        (f32x4.relaxed_min)
        (return (f32x4.extract_lane 0))
    )
    (func (export "test_f32x4_relaxed_max_equal") (result f32)
        (v128.const f32x4 5.0 5.0 5.0 5.0)
        (v128.const f32x4 5.0 5.0 5.0 5.0)
        (f32x4.relaxed_max)
        (return (f32x4.extract_lane 0))
    )

    ;; Large magnitude values
    (func (export "test_f32x4_relaxed_min_large") (result f32)
        (v128.const f32x4 1e30 -1e30 1e30 -1e30)
        (v128.const f32x4 -1e30 1e30 -1e30 1e30)
        (f32x4.relaxed_min)
        (return (f32x4.extract_lane 0))
    )
    (func (export "test_f32x4_relaxed_max_large") (result f32)
        (v128.const f32x4 1e30 -1e30 1e30 -1e30)
        (v128.const f32x4 -1e30 1e30 -1e30 1e30)
        (f32x4.relaxed_max)
        (return (f32x4.extract_lane 0))
    )

    ;; All negative values
    (func (export "test_f32x4_relaxed_min_allneg") (result f32)
        (v128.const f32x4 -1.0 -2.0 -3.0 -4.0)
        (v128.const f32x4 -5.0 -6.0 -7.0 -8.0)
        (f32x4.relaxed_min)
        (return (f32x4.extract_lane 2))
    )
    (func (export "test_f32x4_relaxed_max_allneg") (result f32)
        (v128.const f32x4 -1.0 -2.0 -3.0 -4.0)
        (v128.const f32x4 -5.0 -6.0 -7.0 -8.0)
        (f32x4.relaxed_max)
        (return (f32x4.extract_lane 2))
    )

    ;; f64x2 equal values
    (func (export "test_f64x2_relaxed_min_equal") (result f64)
        (v128.const f64x2 42.0 42.0)
        (v128.const f64x2 42.0 42.0)
        (f64x2.relaxed_min)
        (return (f64x2.extract_lane 0))
    )
    (func (export "test_f64x2_relaxed_max_equal") (result f64)
        (v128.const f64x2 42.0 42.0)
        (v128.const f64x2 42.0 42.0)
        (f64x2.relaxed_max)
        (return (f64x2.extract_lane 0))
    )

    ;; f64x2 large magnitude
    (func (export "test_f64x2_relaxed_min_large") (result f64)
        (v128.const f64x2 1e100 -1e100)
        (v128.const f64x2 -1e100 1e100)
        (f64x2.relaxed_min)
        (return (f64x2.extract_lane 0))
    )
    (func (export "test_f64x2_relaxed_max_large") (result f64)
        (v128.const f64x2 1e100 -1e100)
        (v128.const f64x2 -1e100 1e100)
        (f64x2.relaxed_max)
        (return (f64x2.extract_lane 1))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const {
        test_f32x4_relaxed_min,
        test_f32x4_relaxed_min_lane1,
        test_f32x4_relaxed_max,
        test_f32x4_relaxed_max_lane3,
        test_f64x2_relaxed_min,
        test_f64x2_relaxed_min_lane1,
        test_f64x2_relaxed_max,
        test_f64x2_relaxed_max_lane1,
        test_f32x4_relaxed_min_equal,
        test_f32x4_relaxed_max_equal,
        test_f32x4_relaxed_min_large,
        test_f32x4_relaxed_max_large,
        test_f32x4_relaxed_min_allneg,
        test_f32x4_relaxed_max_allneg,
        test_f64x2_relaxed_min_equal,
        test_f64x2_relaxed_max_equal,
        test_f64x2_relaxed_min_large,
        test_f64x2_relaxed_max_large,
    } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Original tests
        assert.eq(test_f32x4_relaxed_min(), 1.0);
        assert.eq(test_f32x4_relaxed_min_lane1(), -2.0);
        assert.eq(test_f32x4_relaxed_max(), 2.0);
        assert.eq(test_f32x4_relaxed_max_lane3(), 4.0);
        assert.eq(test_f64x2_relaxed_min(), 1.5);
        assert.eq(test_f64x2_relaxed_min_lane1(), -3.5);
        assert.eq(test_f64x2_relaxed_max(), 2.5);
        assert.eq(test_f64x2_relaxed_max_lane1(), -2.5);

        // Equal values
        assert.eq(test_f32x4_relaxed_min_equal(), 5.0);
        assert.eq(test_f32x4_relaxed_max_equal(), 5.0);
        assert.eq(test_f64x2_relaxed_min_equal(), 42.0);
        assert.eq(test_f64x2_relaxed_max_equal(), 42.0);

        // Large magnitude values
        assert.eq(test_f32x4_relaxed_min_large(), Math.fround(-1e30));
        assert.eq(test_f32x4_relaxed_max_large(), Math.fround(1e30));
        assert.eq(test_f64x2_relaxed_min_large(), -1e100);
        assert.eq(test_f64x2_relaxed_max_large(), 1e100);

        // All negative values
        assert.eq(test_f32x4_relaxed_min_allneg(), -7.0);
        assert.eq(test_f32x4_relaxed_max_allneg(), -3.0);
    }
}

await assert.asyncTest(test())
