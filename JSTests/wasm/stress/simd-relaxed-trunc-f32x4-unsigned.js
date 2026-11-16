//@ skip if !$isSIMDPlatform
//@ runDefaultWasm("-m", "--useWasmSIMD=1", "--useWasmRelaxedSIMD=1", "--useWasmIPInt=1", "--useBBQJIT=0", "--useOMGJIT=0")
//@ runDefaultWasm("-m", "--useWasmSIMD=1", "--useWasmRelaxedSIMD=1", "--useWasmIPInt=0", "--useBBQJIT=1", "--useOMGJIT=0", "--useConcurrentJIT=0")
//@ runDefaultWasm("-m", "--useWasmSIMD=1", "--useWasmRelaxedSIMD=1", "--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "relaxed") (param f32) (result i32)
        (i32x4.extract_lane 0
            (i32x4.relaxed_trunc_f32x4_u (f32x4.splat (local.get 0)))))
    (func (export "sat") (param f32) (result i32)
        (i32x4.extract_lane 0
            (i32x4.trunc_sat_f32x4_u (f32x4.splat (local.get 0)))))
    (func (export "mixed_low") (result i32)
        (i32x4.extract_lane 0
            (i32x4.relaxed_trunc_f32x4_u
                (f32x4.replace_lane 1 (f32x4.splat (f32.const 1.9)) (f32.const 3000000000)))))
    (func (export "mixed_high") (result i32)
        (i32x4.extract_lane 1
            (i32x4.relaxed_trunc_f32x4_u
                (f32x4.replace_lane 1 (f32x4.splat (f32.const 1.9)) (f32.const 3000000000)))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, relaxed_simd: true })
    const { relaxed, sat, mixed_low, mixed_high } = instance.exports
    // Exact f32 values inside (-1, 2^32). Repeat so BBQ reaches OMG.
    for (let i = 0; i < 8; ++i) {
        for (let value of [0, 1.9, 2147483520, 2147483648, 3000000000, 4294967040]) {
            assert.eq(relaxed(value) >>> 0, sat(value) >>> 0)
            assert.eq(relaxed(value) >>> 0, value >>> 0)
        }
        assert.eq(mixed_low(), 1)
        assert.eq(mixed_high() >>> 0, 3000000000)
    }
}

await assert.asyncTest(test())
