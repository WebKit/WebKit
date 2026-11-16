//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test vector shift-by-immediate for all lane types, sign modes, and edge cases.
// Constant shifts use NEON SHL/SSHR/USHR immediate instructions on ARM64.
// Dynamic shifts use the mask+splat+SSHL/USHL path.
// Both paths must produce identical results.

// Module 1: i8x16 shifts
async function testI8x16() {
    let wat = `(module
      (memory (export "mem") 1)
      (func (export "shl_3") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shl (v128.load (local.get 0)) (i32.const 3))))
      (func (export "shl_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i8x16.shl (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_u_4") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shr_u (v128.load (local.get 0)) (i32.const 4))))
      (func (export "shr_u_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i8x16.shr_u (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_s_2") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shr_s (v128.load (local.get 0)) (i32.const 2))))
      (func (export "shr_s_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i8x16.shr_s (v128.load (local.get 0)) (local.get 2))))
      (func (export "shl_0") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shl (v128.load (local.get 0)) (i32.const 0))))
      (func (export "shl_7") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shl (v128.load (local.get 0)) (i32.const 7))))
      (func (export "shl_8") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shl (v128.load (local.get 0)) (i32.const 8))))
      (func (export "shl_11") (param i32 i32)
        (v128.store (local.get 1) (i8x16.shl (v128.load (local.get 0)) (i32.const 11))))
    )`
    const instance = await instantiate(wat, {}, { simd: true })
    const u8 = new Uint8Array(instance.exports.mem.buffer)
    const e = instance.exports
    const pattern = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01]
    for (let i = 0; i < 16; i++) u8[i] = pattern[i]

    for (let iter = 0; iter < wasmTestLoopCount; ++iter) {
        // shl const=3 vs dyn=3
        e.shl_3(0, 16)
        e.shl_dyn(0, 32, 3)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        // shr_u const=4 vs dyn=4
        e.shr_u_4(0, 16)
        e.shr_u_dyn(0, 32, 4)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        // shr_s const=2 vs dyn=2
        e.shr_s_2(0, 16)
        e.shr_s_dyn(0, 32, 2)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        // shift 0: identity
        e.shl_0(0, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], pattern[i])

        // shift 8: wraps to 0 (identity)
        e.shl_8(0, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], pattern[i])

        // shift 11 == shift 3
        e.shl_11(0, 16)
        e.shl_3(0, 32)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])
    }
}

// Module 2: i16x8 shifts
async function testI16x8() {
    let wat = `(module
      (memory (export "mem") 1)
      (func (export "shl_5") (param i32 i32)
        (v128.store (local.get 1) (i16x8.shl (v128.load (local.get 0)) (i32.const 5))))
      (func (export "shl_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i16x8.shl (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_u_8") (param i32 i32)
        (v128.store (local.get 1) (i16x8.shr_u (v128.load (local.get 0)) (i32.const 8))))
      (func (export "shr_u_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i16x8.shr_u (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_s_10") (param i32 i32)
        (v128.store (local.get 1) (i16x8.shr_s (v128.load (local.get 0)) (i32.const 10))))
      (func (export "shr_s_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i16x8.shr_s (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_u_0") (param i32 i32)
        (v128.store (local.get 1) (i16x8.shr_u (v128.load (local.get 0)) (i32.const 0))))
      (func (export "shr_u_16") (param i32 i32)
        (v128.store (local.get 1) (i16x8.shr_u (v128.load (local.get 0)) (i32.const 16))))
      (func (export "shr_u_19") (param i32 i32)
        (v128.store (local.get 1) (i16x8.shr_u (v128.load (local.get 0)) (i32.const 19))))
    )`
    const instance = await instantiate(wat, {}, { simd: true })
    const u8 = new Uint8Array(instance.exports.mem.buffer)
    const e = instance.exports
    const pattern = [0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44]
    for (let i = 0; i < 16; i++) u8[i] = pattern[i]

    for (let iter = 0; iter < wasmTestLoopCount; ++iter) {
        e.shl_5(0, 16)
        e.shl_dyn(0, 32, 5)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        e.shr_u_8(0, 16)
        e.shr_u_dyn(0, 32, 8)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        e.shr_s_10(0, 16)
        e.shr_s_dyn(0, 32, 10)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        // shift 0: identity
        e.shr_u_0(0, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], pattern[i])

        // shift 16: wraps to 0
        e.shr_u_16(0, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], pattern[i])

        // shift 19 == shift 3
        e.shr_u_19(0, 16)
        e.shr_u_dyn(0, 32, 3)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])
    }
}

// Module 3: i32x4 shifts
async function testI32x4() {
    let wat = `(module
      (memory (export "mem") 1)
      (func (export "shl_16") (param i32 i32)
        (v128.store (local.get 1) (i32x4.shl (v128.load (local.get 0)) (i32.const 16))))
      (func (export "shl_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i32x4.shl (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_u_12") (param i32 i32)
        (v128.store (local.get 1) (i32x4.shr_u (v128.load (local.get 0)) (i32.const 12))))
      (func (export "shr_u_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i32x4.shr_u (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_s_20") (param i32 i32)
        (v128.store (local.get 1) (i32x4.shr_s (v128.load (local.get 0)) (i32.const 20))))
      (func (export "shr_s_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i32x4.shr_s (v128.load (local.get 0)) (local.get 2))))
      (func (export "shl_0") (param i32 i32)
        (v128.store (local.get 1) (i32x4.shl (v128.load (local.get 0)) (i32.const 0))))
      (func (export "shl_32") (param i32 i32)
        (v128.store (local.get 1) (i32x4.shl (v128.load (local.get 0)) (i32.const 32))))
    )`
    const instance = await instantiate(wat, {}, { simd: true })
    const u8 = new Uint8Array(instance.exports.mem.buffer)
    const mem = new DataView(instance.exports.mem.buffer)
    const e = instance.exports
    const pattern = [0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44]
    for (let i = 0; i < 16; i++) u8[i] = pattern[i]

    for (let iter = 0; iter < wasmTestLoopCount; ++iter) {
        e.shl_16(0, 16)
        e.shl_dyn(0, 32, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        e.shr_u_12(0, 16)
        e.shr_u_dyn(0, 32, 12)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        e.shr_s_20(0, 16)
        e.shr_s_dyn(0, 32, 20)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        // shift 0: identity
        e.shl_0(0, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], pattern[i])

        // shift 32: wraps to 0
        e.shl_32(0, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], pattern[i])
    }
}

// Module 4: i64x2 shifts
async function testI64x2() {
    let wat = `(module
      (memory (export "mem") 1)
      (func (export "shl_32") (param i32 i32)
        (v128.store (local.get 1) (i64x2.shl (v128.load (local.get 0)) (i32.const 32))))
      (func (export "shl_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i64x2.shl (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_u_48") (param i32 i32)
        (v128.store (local.get 1) (i64x2.shr_u (v128.load (local.get 0)) (i32.const 48))))
      (func (export "shr_u_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i64x2.shr_u (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_s_16") (param i32 i32)
        (v128.store (local.get 1) (i64x2.shr_s (v128.load (local.get 0)) (i32.const 16))))
      (func (export "shr_s_dyn") (param i32 i32 i32)
        (v128.store (local.get 1) (i64x2.shr_s (v128.load (local.get 0)) (local.get 2))))
      (func (export "shr_s_0") (param i32 i32)
        (v128.store (local.get 1) (i64x2.shr_s (v128.load (local.get 0)) (i32.const 0))))
      (func (export "shr_s_63") (param i32 i32)
        (v128.store (local.get 1) (i64x2.shr_s (v128.load (local.get 0)) (i32.const 63))))
      (func (export "shr_s_64") (param i32 i32)
        (v128.store (local.get 1) (i64x2.shr_s (v128.load (local.get 0)) (i32.const 64))))
    )`
    const instance = await instantiate(wat, {}, { simd: true })
    const u8 = new Uint8Array(instance.exports.mem.buffer)
    const mem = new DataView(instance.exports.mem.buffer)
    const e = instance.exports

    mem.setBigUint64(0, 0x8000000000000001n, true)
    mem.setBigUint64(8, 0xFFFFFFFFFFFFFFFFn, true)

    for (let iter = 0; iter < wasmTestLoopCount; ++iter) {
        e.shl_32(0, 16)
        e.shl_dyn(0, 32, 32)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        e.shr_u_48(0, 16)
        e.shr_u_dyn(0, 32, 48)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        e.shr_s_16(0, 16)
        e.shr_s_dyn(0, 32, 16)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])

        // shift 0: identity
        e.shr_s_0(0, 16)
        assert.eq(mem.getBigUint64(16, true), 0x8000000000000001n)
        assert.eq(mem.getBigUint64(24, true), 0xFFFFFFFFFFFFFFFFn)

        // shift 64: wraps to 0
        e.shr_s_64(0, 16)
        assert.eq(mem.getBigUint64(16, true), 0x8000000000000001n)
        assert.eq(mem.getBigUint64(24, true), 0xFFFFFFFFFFFFFFFFn)

        // shift 63: max signed right shift
        e.shr_s_63(0, 16)
        e.shr_s_dyn(0, 32, 63)
        for (let i = 0; i < 16; i++) assert.eq(u8[16 + i], u8[32 + i])
    }
}

async function test() {
    await testI8x16()
    await testI16x8()
    await testI32x4()
    await testI64x2()
}

await assert.asyncTest(test())
