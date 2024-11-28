//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param $sz i32) (result i32)
        (i32.const 9)
        (drop)
        (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)
        (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)
        (i8x16.relaxed_swizzle)
        (return (i8x16.extract_lane_u 1))
    )
    (func (export "test2") (param $sz i32) (result i32)
        (i32.const 9)
        (drop)
        (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)
        (v128.const i8x16 0 8 2 3 4 5 6 7 8 9 10 11 12 13 14 15)
        (i8x16.relaxed_swizzle)
        (return (i8x16.extract_lane_u 1))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { test, test2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), 2);
        assert.eq(test2(42), 9);
    }
}

await assert.asyncTest(test())
