//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Regression test for argon2id wasm SIMD miscompilation on x64.
//
// Before the fix, OMGIRGenerator::addSIMDShuffle on x64 split a wasm binary
// i8x16.shuffle into two unary VectorSwizzles (one per source) plus a
// VectorOr, but it kept the original wasm bytes 16..31 in the rightImm
// pattern and relied on PSHUFB's index masking. B3ReduceStrength's
// SwizzleSwizzle composition (composeUnaryShuffle) treats indices ≥ 16 as
// out-of-bounds (ARM64 TBL semantics), so when the rightImm-side unary
// swizzle's input was itself a VectorSwizzle the composed pattern was all
// 0xFF and the resulting PSHUFB returned all zeros. The fix canonicalizes
// rightImm to use 0..15 indices.
//
// The argon2 BLAKE2b inner loop produced this pattern via:
//   i8x16.shuffle <select-from-arg2> arg1 (i8x16.shuffle <rotr16> b b)
// which after the buggy split-and-compose miscomputed the multiplier input
// as zero, propagating wrong bytes through the rest of the BLAKE2b round.
//
// This test reproduces the smallest pattern shape under OMG and asserts
// against the expected byte result.

async function test() {
    // Two wasm i8x16.shuffles. The outer takes (anything, inner_result) and
    // selects only bytes from arg2 (indices 16..31). The inner is a unary
    // shuffle over `b`. Without the canonicalization fix, the outer's split
    // rightImm has bytes 16..31 in the B3 IR; SwizzleSwizzle composition
    // (since the rightImm-side's child(0) is the inner VectorSwizzle on x64)
    // produces composed = all 0xFF and PSHUFB returns all zeros.
    let wat = `(module
      (memory (export "mem") 1)

      (func (export "compose_test") (param $aPtr i32) (param $bPtr i32) (param $outPtr i32)
        (local.set $aPtr (local.get $aPtr))
        (v128.store (local.get $outPtr)
          ;; outer: take bytes 16..31 of (a, inner_result) → identity-of-arg2.
          (i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
            (v128.load (local.get $aPtr))
            ;; inner: rotr16 of b within each i64x2 lane.
            (i8x16.shuffle 2 3 4 5 6 7 0 1 10 11 12 13 14 15 8 9
              (v128.load (local.get $bPtr))
              (v128.load (local.get $bPtr))))))
    )`

    const instance = await instantiate(wat, { }, { simd: true })
    const memory = new Uint8Array(instance.exports.mem.buffer)

    // Set 'a' to a distinguishable pattern (will be discarded by outer shuffle).
    for (let i = 0; i < 16; ++i)
        memory[i] = 0xA0 + i
    // Set 'b' to 0x10..0x1F so the rotr16 result is easy to verify.
    for (let i = 0; i < 16; ++i)
        memory[16 + i] = 0x10 + i

    const expected = new Uint8Array(16)
    // rotr16 byte pattern: {2,3,4,5,6,7,0,1, 10,11,12,13,14,15,8,9}
    const rotr16 = [2,3,4,5,6,7,0,1, 10,11,12,13,14,15,8,9]
    for (let i = 0; i < 16; ++i)
        expected[i] = 0x10 + rotr16[i]

    // Trigger OMG by running many times.
    const loopCount = (typeof wasmTestLoopCount === 'number') ? wasmTestLoopCount : 1000
    for (let iter = 0; iter < loopCount; ++iter) {
        instance.exports.compose_test(0, 16, 32)
        for (let i = 0; i < 16; ++i)
            assert.eq(memory[32 + i], expected[i], `byte ${i} at iteration ${iter}`)
    }
}

await assert.asyncTest(test())
