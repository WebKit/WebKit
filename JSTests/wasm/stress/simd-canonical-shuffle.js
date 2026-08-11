//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Comprehensive test for binary shuffle canonical pattern recognition.
// These patterns should be lowered to UZP1/UZP2/ZIP1/ZIP2/TRN1/TRN2/REV instructions.

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; UZP1.4S binary: {a[0],a[2],b[0],b[2]}
      (func (export "uzp1_4s") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 0 1 2 3 8 9 10 11 16 17 18 19 24 25 26 27
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; UZP2.4S binary: {a[1],a[3],b[1],b[3]}
      (func (export "uzp2_4s") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 4 5 6 7 12 13 14 15 20 21 22 23 28 29 30 31
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; ZIP1.4S binary: {a[0],b[0],a[1],b[1]}
      (func (export "zip1_4s") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 0 1 2 3 16 17 18 19 4 5 6 7 20 21 22 23
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; ZIP2.4S binary: {a[2],b[2],a[3],b[3]}
      (func (export "zip2_4s") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 8 9 10 11 24 25 26 27 12 13 14 15 28 29 30 31
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; TRN1.4S binary: {a[0],b[0],a[2],b[2]}
      (func (export "trn1_4s") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 0 1 2 3 16 17 18 19 8 9 10 11 24 25 26 27
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; TRN2.4S binary: {a[1],b[1],a[3],b[3]}
      (func (export "trn2_4s") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 4 5 6 7 20 21 22 23 12 13 14 15 28 29 30 31
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; UZP1.2D binary: {a[lo64],b[lo64]}
      (func (export "uzp1_2d") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; UZP2.2D binary: {a[hi64],b[hi64]}
      (func (export "uzp2_2d") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; Unary REV64.4S: swap 32-bit pairs within 64-bit lanes
      (func (export "rev64_4s_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; Unary REV32.8H: swap 16-bit elements within 32-bit groups
      (func (export "rev32_8h_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 2 3 0 1 6 7 4 5 10 11 8 9 14 15 12 13
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; Unary REV16.16B: swap bytes within 16-bit groups
      (func (export "rev16_16b_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 1 0 3 2 5 4 7 6 9 8 11 10 13 12 15 14
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; Unary UZP1.4S(v,v): {v[0],v[2],v[0],v[2]}
      (func (export "uzp1_4s_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 0 1 2 3 8 9 10 11 0 1 2 3 8 9 10 11
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; Unary TRN1.4S(v,v): {v[0],v[0],v[2],v[2]}
      (func (export "trn1_4s_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 0 1 2 3 0 1 2 3 8 9 10 11 8 9 10 11
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new DataView(instance.exports.mem.buffer)

    function setI32x4(offset, a, b, c, d) {
        mem.setUint32(offset, a, true)
        mem.setUint32(offset+4, b, true)
        mem.setUint32(offset+8, c, true)
        mem.setUint32(offset+12, d, true)
    }
    function getI32x4(offset) {
        return [mem.getUint32(offset, true), mem.getUint32(offset+4, true),
                mem.getUint32(offset+8, true), mem.getUint32(offset+12, true)]
    }
    function setBytes(offset, arr) {
        const u8 = new Uint8Array(instance.exports.mem.buffer)
        for (let i = 0; i < arr.length; i++) u8[offset+i] = arr[i]
    }
    function getBytes(offset, len) {
        const u8 = new Uint8Array(instance.exports.mem.buffer)
        return Array.from(u8.slice(offset, offset+len))
    }

    // Setup: a = {0x11, 0x22, 0x33, 0x44}, b = {0x55, 0x66, 0x77, 0x88}
    setI32x4(0, 0x11, 0x22, 0x33, 0x44)
    setI32x4(16, 0x55, 0x66, 0x77, 0x88)

    function verify(name, fn, args, expected) {
        fn(...args)
        let got = getI32x4(args[args.length-1])
        assert.eq(got.join(','), expected.join(','), name)
    }

    // Binary patterns
    verify("UZP1.4S", instance.exports.uzp1_4s, [0,16,32], [0x11,0x33,0x55,0x77])
    verify("UZP2.4S", instance.exports.uzp2_4s, [0,16,32], [0x22,0x44,0x66,0x88])
    verify("ZIP1.4S", instance.exports.zip1_4s, [0,16,32], [0x11,0x55,0x22,0x66])
    verify("ZIP2.4S", instance.exports.zip2_4s, [0,16,32], [0x33,0x77,0x44,0x88])
    verify("TRN1.4S", instance.exports.trn1_4s, [0,16,32], [0x11,0x55,0x33,0x77])
    verify("TRN2.4S", instance.exports.trn2_4s, [0,16,32], [0x22,0x66,0x44,0x88])
    verify("UZP1.2D", instance.exports.uzp1_2d, [0,16,32], [0x11,0x22,0x55,0x66])
    verify("UZP2.2D", instance.exports.uzp2_2d, [0,16,32], [0x33,0x44,0x77,0x88])

    // Unary patterns
    verify("REV64.4S unary", instance.exports.rev64_4s_unary, [0,32], [0x22,0x11,0x44,0x33])
    verify("UZP1.4S unary", instance.exports.uzp1_4s_unary, [0,32], [0x11,0x33,0x11,0x33])
    verify("TRN1.4S unary", instance.exports.trn1_4s_unary, [0,32], [0x11,0x11,0x33,0x33])

    // Byte-level tests
    setBytes(0, [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15])
    instance.exports.rev32_8h_unary(0, 32)
    assert.eq(getBytes(32, 16).join(','), '2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13', 'REV32.8H')

    instance.exports.rev16_16b_unary(0, 32)
    assert.eq(getBytes(32, 16).join(','), '1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14', 'REV16.16B')

    // Trigger OMG and re-verify
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.uzp1_4s(0, 16, 32)
        instance.exports.uzp2_4s(0, 16, 32)
        instance.exports.zip1_4s(0, 16, 32)
        instance.exports.zip2_4s(0, 16, 32)
        instance.exports.trn1_4s(0, 16, 32)
        instance.exports.trn2_4s(0, 16, 32)
        instance.exports.rev64_4s_unary(0, 32)
        instance.exports.rev32_8h_unary(0, 32)
        instance.exports.rev16_16b_unary(0, 32)
        instance.exports.uzp1_4s_unary(0, 32)
        instance.exports.trn1_4s_unary(0, 32)
    }

    // Re-verify after OMG
    setI32x4(0, 0x11, 0x22, 0x33, 0x44)
    setI32x4(16, 0x55, 0x66, 0x77, 0x88)
    verify("UZP1.4S post-OMG", instance.exports.uzp1_4s, [0,16,32], [0x11,0x33,0x55,0x77])
    verify("ZIP1.4S post-OMG", instance.exports.zip1_4s, [0,16,32], [0x11,0x55,0x22,0x66])
    verify("TRN1.4S post-OMG", instance.exports.trn1_4s, [0,16,32], [0x11,0x55,0x33,0x77])
    verify("REV64.4S post-OMG", instance.exports.rev64_4s_unary, [0,32], [0x22,0x11,0x44,0x33])

    setBytes(0, [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15])
    instance.exports.rev16_16b_unary(0, 32)
    assert.eq(getBytes(32, 16).join(','), '1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14', 'REV16.16B post-OMG')
}

await assert.asyncTest(test())
