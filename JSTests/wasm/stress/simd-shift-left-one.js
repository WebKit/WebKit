//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test that i64x2.shl by constant 1 is optimized to i64x2.add(x, x).
// This is a strength reduction: x << 1 == x + x.

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; Simple shift left by 1
      (func (export "shl1") (param i32 i32)
        (v128.store (local.get 1)
          (i64x2.shl (v128.load (local.get 0)) (i32.const 1))))

      ;; Multiply-and-double pattern (common in BLAKE2b):
      ;; (lo32(a) * lo32(b)) << 1
      (func (export "mul_double") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i64x2.shl
            (i64x2.mul (v128.load (local.get 0)) (v128.load (local.get 1)))
            (i32.const 1))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new DataView(instance.exports.mem.buffer)

    // Test shl1: [3, 7] << 1 = [6, 14]
    mem.setBigUint64(0, 3n, true)
    mem.setBigUint64(8, 7n, true)
    instance.exports.shl1(0, 16)
    assert.eq(mem.getBigUint64(16, true), 6n)
    assert.eq(mem.getBigUint64(24, true), 14n)

    // Test with large values near overflow
    mem.setBigUint64(0, 0x8000000000000000n, true)
    mem.setBigUint64(8, 0xFFFFFFFFFFFFFFFFn, true)
    instance.exports.shl1(0, 16)
    assert.eq(mem.getBigUint64(16, true), 0n) // overflow wraps
    assert.eq(mem.getBigUint64(24, true), 0xFFFFFFFFFFFFFFFEn)

    // Test mul_double: [5, 3] * [4, 6] << 1 = [40, 36]
    mem.setBigUint64(0, 5n, true)
    mem.setBigUint64(8, 3n, true)
    mem.setBigUint64(16, 4n, true)
    mem.setBigUint64(24, 6n, true)
    instance.exports.mul_double(0, 16, 32)
    assert.eq(mem.getBigUint64(32, true), 40n)
    assert.eq(mem.getBigUint64(40, true), 36n)

    // Run many times to trigger OMG
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.shl1(0, 16)
        instance.exports.mul_double(0, 16, 32)
    }

    // Re-verify after OMG
    mem.setBigUint64(0, 3n, true)
    mem.setBigUint64(8, 7n, true)
    instance.exports.shl1(0, 16)
    assert.eq(mem.getBigUint64(16, true), 6n)
    assert.eq(mem.getBigUint64(24, true), 14n)
}

await assert.asyncTest(test())
