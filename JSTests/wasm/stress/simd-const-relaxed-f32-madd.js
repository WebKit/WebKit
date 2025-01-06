//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param $sz i32) (result f32)
        (i32.const 9)
        (drop)
        (v128.const f32x4 5.0  4.0  -37.0 6.0)
        (v128.const f32x4 22.0 25.0 -3.0  20.0)
        (v128.const f32x4 -1.0 1.0  0.0   -1.0)
        (f32x4.relaxed_madd)
        (return (f32x4.extract_lane 1))
    )
    (func (export "test2") (param $sz i32) (result f32)
        (i32.const 9)
        (drop)
        (v128.const f32x4 5.0   4.0   -37.0 -6.0)
        (v128.const f32x4 -22.0 -25.0 3.0   20.0)
        (v128.const f32x4 -1.0  1.0   0.0   -1.0)
        (f32x4.relaxed_nmadd)
        (return (f32x4.extract_lane 3))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { test, test2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), 101.0);
        assert.eq(test2(42), 119.0);
    }
}

await assert.asyncTest(test())
