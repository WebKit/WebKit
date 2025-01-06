//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param $sz i32) (result f64)
        (i32.const 9)
        (drop)
        (v128.const f64x2 28.0 24.0)
        (v128.const f64x2 10.0 20.0)
        (v128.const f64x2 1.0  2.0)
        (f64x2.relaxed_madd)
        (return (f64x2.extract_lane 0))
    )
    (func (export "test2") (param $sz i32) (result f64)
        (i32.const 9)
        (drop)
        (v128.const f64x2 28.0 24.0)
        (v128.const f64x2 10.0 20.0)
        (v128.const f64x2 -1.0  -2.0)
        (f64x2.relaxed_nmadd)
        (return (f64x2.extract_lane 1))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { test, test2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), 281.0);
        assert.eq(test2(42), -482.0);
    }
}

await assert.asyncTest(test())
