//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "tests1") (param $sz i32) (result i32)
        (i32.const 9)
        (drop)
        (v128.const f64x2 1.5 -1.5)
        (i32x4.relaxed_trunc_f64x2_s_zero)
        (return (i32x4.extract_lane 1))
    )
    (func (export "tests2") (param $sz i32) (result i32)
        (v128.const f64x2 -1.4 -2.3)
        (i32x4.relaxed_trunc_f64x2_s_zero)
        (return (i32x4.extract_lane 2))
    )

    (func (export "testu1") (param $sz i32) (result i32)
        (v128.const f64x2 1.5 2.4)
        (i32x4.relaxed_trunc_f64x2_u_zero)
        (return (i32x4.extract_lane 1))
    )

    (func (export "testu2") (param $sz i32) (result i32)
        (v128.const f64x2 14.0 1.39)
        (i32x4.relaxed_trunc_f64x2_u_zero)
        (return (i32x4.extract_lane 2))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { tests1, tests2, testu1, testu2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(tests1(42), -1)
        assert.eq(tests2(48), 0);
        assert.eq(testu1(48), 2);
        assert.eq(testu2(48), 0);
    }
}

await assert.asyncTest(test())
