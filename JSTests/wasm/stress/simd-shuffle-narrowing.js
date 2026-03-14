//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that shuffle patterns feeding extend_low / extmul_low are correctly compiled
// (the B3 ReduceSIMDShuffle phase may narrow them to VectorSwizzle8).

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; shuffle(x, x, {0,1,2,3,8,9,10,11,...}) → i64x2.extend_low_i32x4_u
      ;; Extracts i32 elements [0] and [2], then zero-extends to i64x2.
      (func (export "shuffle_extend_low") (param i32)
        (v128.store (i32.const 16)
          (i64x2.extend_low_i32x4_u
            (i8x16.shuffle 0 1 2 3 8 9 10 11 0 1 2 3 8 9 10 11
              (v128.load (local.get 0))
              (v128.load (local.get 0))))))

      ;; shuffle + i64x2.extmul_low_i32x4_u
      (func (export "shuffle_extmul_low") (param i32 i32)
        (v128.store (i32.const 32)
          (i64x2.extmul_low_i32x4_u
            (i8x16.shuffle 0 1 2 3 8 9 10 11 0 1 2 3 8 9 10 11
              (v128.load (local.get 0))
              (v128.load (local.get 0)))
            (i8x16.shuffle 0 1 2 3 8 9 10 11 0 1 2 3 8 9 10 11
              (v128.load (local.get 1))
              (v128.load (local.get 1))))))

      ;; shuffle → i32x4.extract_lane 0
      (func (export "shuffle_extract_lane0") (param i32) (result i32)
        (i32x4.extract_lane 0
          (i8x16.shuffle 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new DataView(instance.exports.mem.buffer)

    // Test shuffle_extend_low: input [1, 2, 3, 4] as i32x4
    // shuffle picks elements [0]=1 and [2]=3 → extend_low → [1n, 3n]
    mem.setUint32(0, 1, true)
    mem.setUint32(4, 2, true)
    mem.setUint32(8, 3, true)
    mem.setUint32(12, 4, true)
    instance.exports.shuffle_extend_low(0)
    assert.eq(mem.getBigUint64(16, true), 1n)
    assert.eq(mem.getBigUint64(24, true), 3n)

    // Test shuffle_extmul_low: a=[2,0,3,0], b=[5,0,7,0]
    // shuffle a→[2,3], shuffle b→[5,7], extmul→[10,21]
    mem.setUint32(0, 2, true)
    mem.setUint32(4, 0, true)
    mem.setUint32(8, 3, true)
    mem.setUint32(12, 0, true)
    mem.setUint32(48, 5, true)
    mem.setUint32(52, 0, true)
    mem.setUint32(56, 7, true)
    mem.setUint32(60, 0, true)
    instance.exports.shuffle_extmul_low(0, 48)
    assert.eq(mem.getBigUint64(32, true), 10n)
    assert.eq(mem.getBigUint64(40, true), 21n)

    // Test shuffle_extract_lane0: input [10, 20, 30, 40]
    // shuffle {4,5,6,7,...} puts element [1]=20 at lane 0 → extract → 20
    mem.setUint32(0, 10, true)
    mem.setUint32(4, 20, true)
    mem.setUint32(8, 30, true)
    mem.setUint32(12, 40, true)
    assert.eq(instance.exports.shuffle_extract_lane0(0), 20)

    // Run many iterations for OMG compilation
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.shuffle_extend_low(0)
        instance.exports.shuffle_extmul_low(0, 48)
        instance.exports.shuffle_extract_lane0(0)
    }

    // Verify correctness after optimization
    assert.eq(instance.exports.shuffle_extract_lane0(0), 20)
}

await assert.asyncTest(test())
