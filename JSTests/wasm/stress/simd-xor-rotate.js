//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test XOR-and-rotate-right patterns for i64x2.
// On ARM64 with SHA3 support, these should be lowered to XAR instructions.
// Pattern: (v128.or (i64x2.shl (v128.xor a b) (64-n)) (i64x2.shr_u (v128.xor a b) n))

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; XOR + rotate right by 32 (BLAKE2b rotation)
      (func (export "xor_rotr32") (param i32 i32 i32)
        (v128.store (local.get 2)
          (v128.or
            (i64x2.shl
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 32))
            (i64x2.shr_u
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 32)))))

      ;; XOR + rotate right by 24 (BLAKE2b rotation)
      (func (export "xor_rotr24") (param i32 i32 i32)
        (v128.store (local.get 2)
          (v128.or
            (i64x2.shl
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 40))
            (i64x2.shr_u
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 24)))))

      ;; XOR + rotate right by 16 (BLAKE2b rotation)
      (func (export "xor_rotr16") (param i32 i32 i32)
        (v128.store (local.get 2)
          (v128.or
            (i64x2.shl
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 48))
            (i64x2.shr_u
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 16)))))

      ;; XOR + rotate right by 63 (BLAKE2b rotation)
      (func (export "xor_rotr63") (param i32 i32 i32)
        (v128.store (local.get 2)
          (v128.or
            (i64x2.shl
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 1))
            (i64x2.shr_u
              (v128.xor (v128.load (local.get 0)) (v128.load (local.get 1)))
              (i32.const 63)))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new DataView(instance.exports.mem.buffer)
    const mem8 = new Uint8Array(instance.exports.mem.buffer)

    // Set up test vectors at offsets 0 and 16.
    // a = [0x0123456789ABCDEF, 0xFEDCBA9876543210]
    // b = [0xFF00FF00FF00FF00, 0x00FF00FF00FF00FF]
    mem.setBigUint64(0, 0x0123456789ABCDEFn, true)
    mem.setBigUint64(8, 0xFEDCBA9876543210n, true)
    mem.setBigUint64(16, 0xFF00FF00FF00FF00n, true)
    mem.setBigUint64(24, 0x00FF00FF00FF00FFn, true)

    function rotr64(val, n) {
        n = BigInt(n)
        return ((val >> n) | (val << (64n - n))) & 0xFFFFFFFFFFFFFFFFn
    }

    let xor0 = 0x0123456789ABCDEFn ^ 0xFF00FF00FF00FF00n
    let xor1 = 0xFEDCBA9876543210n ^ 0x00FF00FF00FF00FFn

    function verify(funcName, rotateAmount) {
        let expected0 = rotr64(xor0, rotateAmount)
        let expected1 = rotr64(xor1, rotateAmount)
        let got0 = mem.getBigUint64(32, true)
        let got1 = mem.getBigUint64(40, true)
        assert.eq(got0, expected0, `${funcName} lane 0`)
        assert.eq(got1, expected1, `${funcName} lane 1`)
    }

    // Test each rotation
    instance.exports.xor_rotr32(0, 16, 32)
    verify("xor_rotr32", 32)

    instance.exports.xor_rotr24(0, 16, 32)
    verify("xor_rotr24", 24)

    instance.exports.xor_rotr16(0, 16, 32)
    verify("xor_rotr16", 16)

    instance.exports.xor_rotr63(0, 16, 32)
    verify("xor_rotr63", 63)

    // Run many times to trigger OMG
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.xor_rotr32(0, 16, 32)
        instance.exports.xor_rotr24(0, 16, 32)
        instance.exports.xor_rotr16(0, 16, 32)
        instance.exports.xor_rotr63(0, 16, 32)
    }

    // Re-verify after OMG optimization
    instance.exports.xor_rotr32(0, 16, 32)
    verify("xor_rotr32 (post-OMG)", 32)

    instance.exports.xor_rotr24(0, 16, 32)
    verify("xor_rotr24 (post-OMG)", 24)

    instance.exports.xor_rotr16(0, 16, 32)
    verify("xor_rotr16 (post-OMG)", 16)

    instance.exports.xor_rotr63(0, 16, 32)
    verify("xor_rotr63 (post-OMG)", 63)
}

await assert.asyncTest(test())
