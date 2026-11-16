//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test chained shuffle composition (shuffle-of-shuffle optimization)
// and same-child binary shuffle normalization.

async function test() {
    let wat = `(module
      (memory (export "mem") 1)

      ;; unpackhi(a, unpacklo(b, b)):
      ;; Inner: shuffle(b, b, {0..7, 16..23}) = dup low qword of b
      ;; Outer: shuffle(a, inner, {8..15, 24..31}) = {a[8..15], inner[8..15]=[b[0..7]]}
      ;; Composed result: {a[8..15], b[0..7]}
      (func (export "unpackhi_unpacklo") (param i32 i32)
        (v128.store (i32.const 32)
          (i8x16.shuffle 8 9 10 11 12 13 14 15 24 25 26 27 28 29 30 31
            (v128.load (local.get 0))
            (i8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23
              (v128.load (local.get 1))
              (v128.load (local.get 1))))))

      ;; Same child on both sides: shuffle(x, x, {0..7, 16..23}) → dup low qword
      (func (export "dup_low_qword") (param i32)
        (v128.store (i32.const 16)
          (i8x16.shuffle 0 1 2 3 4 5 6 7 16 17 18 19 20 21 22 23
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))

      ;; Identity shuffle
      (func (export "identity") (param i32)
        (v128.store (i32.const 16)
          (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
            (v128.load (local.get 0))
            (v128.load (local.get 0)))))
    )`

    const instance = await instantiate(wat, {}, { simd: true })
    const mem = new Uint8Array(instance.exports.mem.buffer)

    // Set up: a = [0..15] at offset 0, b = [16..31] at offset 64
    for (let i = 0; i < 16; i++) {
        mem[i] = i
        mem[64 + i] = 16 + i
    }

    function getResult(offset, len) {
        return Array.from(mem.slice(offset, offset + len)).join(',')
    }

    // unpackhi_unpacklo: {a[8..15], b[0..7]}
    instance.exports.unpackhi_unpacklo(0, 64)
    assert.eq(getResult(32, 16), '8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23')

    // dup_low_qword: {a[0..7], a[0..7]}
    instance.exports.dup_low_qword(0)
    assert.eq(getResult(16, 16), '0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7')

    // identity: unchanged
    instance.exports.identity(0)
    assert.eq(getResult(16, 16), '0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15')

    // Run many times to trigger OMG
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        instance.exports.unpackhi_unpacklo(0, 64)
        instance.exports.dup_low_qword(0)
        instance.exports.identity(0)
    }

    // Verify correctness after OMG
    instance.exports.unpackhi_unpacklo(0, 64)
    assert.eq(getResult(32, 16), '8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23')
}

await assert.asyncTest(test())
