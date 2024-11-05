//@ requireOptions("--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test1") (result i32)
        (i8x16.relaxed_laneselect (v128.const i8x16 0 127 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 7 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
        (return (i8x16.extract_lane_u 1))
    )
    (func (export "test2") (result i32)
        (i16x8.relaxed_laneselect (v128.const i16x8 0 32767 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 7 0 0 0 0 0 0))
        (return (i16x8.extract_lane_u 1))
    )
    (func (export "test3") (result i32)
        (i32x4.relaxed_laneselect (v128.const i32x4 0 2147483647 0 0) (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 7 0 0))
        (return (i32x4.extract_lane 1))
    )
    (func (export "test4") (result i64)
        (i64x2.relaxed_laneselect (v128.const i64x2 0 9223372036854775807) (v128.const i64x2 0 0) (v128.const i64x2 0 7))
        (return (i64x2.extract_lane 1))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { test1, test2, test3, test4 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test1(), 7);
        assert.eq(test2(), 7);
        assert.eq(test3(), 7);
        assert.eq(test4(), 7n);
    }
}

await assert.asyncTest(test())
