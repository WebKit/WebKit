//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test unary shuffle patterns that should be matched as UZP1/UZP2/ZIP1/ZIP2
// with the same register on both inputs (unary-as-binary canonical matching).

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; UZP1.4S(v, v): extract even 32-bit elements {v[0], v[2], v[0], v[2]}
      ;; Pattern: {0,1,2,3, 8,9,10,11, 0,1,2,3, 8,9,10,11}
      (func (export "uzp1_4s_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 0 1 2 3 8 9 10 11 0 1 2 3 8 9 10 11
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; UZP2.4S(v, v): extract odd 32-bit elements {v[1], v[3], v[1], v[3]}
      ;; Pattern: {4,5,6,7, 12,13,14,15, 4,5,6,7, 12,13,14,15}
      (func (export "uzp2_4s_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 4 5 6 7 12 13 14 15 4 5 6 7 12 13 14 15
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; UZP1.2D(v, v): duplicate low 64-bit element {v[0], v[0]}
      ;; Pattern: {0,1,2,3,4,5,6,7, 0,1,2,3,4,5,6,7}
      (func (export "uzp1_2d_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; UZP2.2D(v, v): duplicate high 64-bit element {v[1], v[1]}
      ;; Pattern: {8,9,10,11,12,13,14,15, 8,9,10,11,12,13,14,15}
      (func (export "uzp2_2d_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; ZIP1.4S(v, v): interleave low 32-bit halves {v[0], v[0], v[1], v[1]}
      ;; Pattern: {0,1,2,3, 0,1,2,3, 4,5,6,7, 4,5,6,7}
      (func (export "zip1_4s_unary") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle 0 1 2 3 0 1 2 3 4 5 6 7 4 5 6 7
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new Uint8Array(instance.exports.mem.buffer)
    const memView = new DataView(instance.exports.mem.buffer)

    // Set up: v = [0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00] as i32x4
    memView.setUint32(0, 0x11223344, true)
    memView.setUint32(4, 0x55667788, true)
    memView.setUint32(8, 0x99AABBCC, true)
    memView.setUint32(12, 0xDDEEFF00, true)

    function getResult32(offset) {
        return [
            memView.getUint32(offset, true),
            memView.getUint32(offset + 4, true),
            memView.getUint32(offset + 8, true),
            memView.getUint32(offset + 12, true)
        ].map(x => '0x' + x.toString(16).padStart(8, '0')).join(',')
    }

    // UZP1.4S: {v[0], v[2], v[0], v[2]}
    instance.exports.uzp1_4s_unary(0, 16)
    assert.eq(getResult32(16), '0x11223344,0x99aabbcc,0x11223344,0x99aabbcc')

    // UZP2.4S: {v[1], v[3], v[1], v[3]}
    instance.exports.uzp2_4s_unary(0, 16)
    assert.eq(getResult32(16), '0x55667788,0xddeeff00,0x55667788,0xddeeff00')

    // UZP1.2D: {v[0..1], v[0..1]}
    instance.exports.uzp1_2d_unary(0, 16)
    assert.eq(getResult32(16), '0x11223344,0x55667788,0x11223344,0x55667788')

    // UZP2.2D: {v[2..3], v[2..3]}
    instance.exports.uzp2_2d_unary(0, 16)
    assert.eq(getResult32(16), '0x99aabbcc,0xddeeff00,0x99aabbcc,0xddeeff00')

    // ZIP1.4S: {v[0], v[0], v[1], v[1]}
    instance.exports.zip1_4s_unary(0, 16)
    assert.eq(getResult32(16), '0x11223344,0x11223344,0x55667788,0x55667788')

    // Run many times to trigger OMG
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.uzp1_4s_unary(0, 16)
        instance.exports.uzp2_4s_unary(0, 16)
        instance.exports.uzp1_2d_unary(0, 16)
        instance.exports.uzp2_2d_unary(0, 16)
        instance.exports.zip1_4s_unary(0, 16)
    }

    // Re-verify after OMG
    instance.exports.uzp1_4s_unary(0, 16)
    assert.eq(getResult32(16), '0x11223344,0x99aabbcc,0x11223344,0x99aabbcc')

    instance.exports.uzp2_4s_unary(0, 16)
    assert.eq(getResult32(16), '0x55667788,0xddeeff00,0x55667788,0xddeeff00')

    instance.exports.uzp1_2d_unary(0, 16)
    assert.eq(getResult32(16), '0x11223344,0x55667788,0x11223344,0x55667788')
}

await assert.asyncTest(test())
