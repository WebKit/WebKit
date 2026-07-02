//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test binary shuffle patterns recognized as EXT, UZP, ZIP instructions.

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; EXT #8: bytes {8,...,15, 16,...,23}
      (func (export "ext8") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; EXT #4
      (func (export "ext4") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; UZP1.2D: {a[0..7], b[0..7]}
      (func (export "uzp1_64") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; UZP2.2D: {a[8..15], b[8..15]}
      (func (export "uzp2_64") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; ZIP1.4S: {a[0],b[0],a[1],b[1]}
      (func (export "zip1_32") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 0 1 2 3 16 17 18 19 4 5 6 7 20 21 22 23
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; ZIP2.4S: {a[2],b[2],a[3],b[3]}
      (func (export "zip2_32") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 8 9 10 11 24 25 26 27 12 13 14 15 28 29 30 31
            (v128.load (local.get 0)) (v128.load (local.get 1)))))

      ;; UZP1.4S: {a[0],a[2],b[0],b[2]}
      (func (export "uzp1_32") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 0 1 2 3 8 9 10 11 16 17 18 19 24 25 26 27
            (v128.load (local.get 0)) (v128.load (local.get 1)))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new Uint8Array(instance.exports.mem.buffer)

    // Set up test vectors: a = [0..15], b = [16..31]
    for (let i = 0; i < 16; i++) {
        mem[i] = i
        mem[16 + i] = 16 + i
    }

    function getResult() {
        return Array.from(mem.slice(32, 48)).join(',')
    }

    // EXT #8
    instance.exports.ext8(0, 16)
    assert.eq(getResult(), '8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23')

    // EXT #4
    instance.exports.ext4(0, 16)
    assert.eq(getResult(), '4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19')

    // UZP1.2D
    instance.exports.uzp1_64(0, 16)
    assert.eq(getResult(), '0,1,2,3,4,5,6,7,16,17,18,19,20,21,22,23')

    // UZP2.2D
    instance.exports.uzp2_64(0, 16)
    assert.eq(getResult(), '8,9,10,11,12,13,14,15,24,25,26,27,28,29,30,31')

    // ZIP1.4S
    instance.exports.zip1_32(0, 16)
    assert.eq(getResult(), '0,1,2,3,16,17,18,19,4,5,6,7,20,21,22,23')

    // ZIP2.4S
    instance.exports.zip2_32(0, 16)
    assert.eq(getResult(), '8,9,10,11,24,25,26,27,12,13,14,15,28,29,30,31')

    // UZP1.4S
    instance.exports.uzp1_32(0, 16)
    assert.eq(getResult(), '0,1,2,3,8,9,10,11,16,17,18,19,24,25,26,27')

    // Run many times for OMG
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.ext8(0, 16)
        instance.exports.uzp1_64(0, 16)
        instance.exports.zip1_32(0, 16)
    }

    // Re-verify after optimization
    instance.exports.ext8(0, 16)
    assert.eq(getResult(), '8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23')
}

await assert.asyncTest(test())
