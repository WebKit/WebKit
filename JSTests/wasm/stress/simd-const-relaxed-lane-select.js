//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// relaxed_laneselect: result = (mask & a) | (~mask & b) on ARM (bitwise),
// or mask.highbit ? a : b on x86 (top-bit-only).
// Use all-ones/all-zeros mask bytes so both interpretations agree.
let wat = `
(module
    (func (export "test1") (result i32)
        (i8x16.relaxed_laneselect (v128.const i8x16 0 127 0 0 0 0 0 0 0 0 0 0 0 0 0 42) (v128.const i8x16 99 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 255 0 0 0 0 0 0 0 0 0 0 0 0 0 255))
        (return (i8x16.extract_lane_u 1))
    )
    (func (export "test1_false") (result i32)
        (i8x16.relaxed_laneselect (v128.const i8x16 0 127 0 0 0 0 0 0 0 0 0 0 0 0 0 42) (v128.const i8x16 99 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 255 0 0 0 0 0 0 0 0 0 0 0 0 0 255))
        (return (i8x16.extract_lane_u 0))
    )
    (func (export "test1_lane15") (result i32)
        (i8x16.relaxed_laneselect (v128.const i8x16 0 127 0 0 0 0 0 0 0 0 0 0 0 0 0 42) (v128.const i8x16 99 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 255 0 0 0 0 0 0 0 0 0 0 0 0 0 255))
        (return (i8x16.extract_lane_u 15))
    )
    (func (export "test2") (result i32)
        (i16x8.relaxed_laneselect (v128.const i16x8 0 32767 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 -1 0 0 0 0 0 0))
        (return (i16x8.extract_lane_u 1))
    )
    (func (export "test3") (result i32)
        (i32x4.relaxed_laneselect (v128.const i32x4 0 2147483647 0 0) (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 -1 0 0))
        (return (i32x4.extract_lane 1))
    )
    (func (export "test4") (result i64)
        (i64x2.relaxed_laneselect (v128.const i64x2 0 9223372036854775807) (v128.const i64x2 0 0) (v128.const i64x2 0 -1))
        (return (i64x2.extract_lane 1))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { test1, test1_false, test1_lane15, test2, test3, test4 } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // mask=0xFF selects from a (lane 1: 127)
        assert.eq(test1(), 127);
        // mask=0x00 selects from b (lane 0: 99)
        assert.eq(test1_false(), 99);
        // mask=0xFF selects from a (lane 15: 42)
        assert.eq(test1_lane15(), 42);
        assert.eq(test2(), 32767);
        assert.eq(test3(), 2147483647);
        assert.eq(test4(), 9223372036854775807n);
    }
}

await assert.asyncTest(test())
