import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Test SIMD memory operations through the fast/slow path memarg parsing.

let wat = `
(module
    (memory 1)

    ;; Initialize memory with known pattern
    (data (i32.const 0) "\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1a\\1b\\1c\\1d\\1e\\1f\\20")

    ;; v128.load + v128.store round-trip
    (func (export "test_load_store") (result i32)
        ;; Load v128 from offset 0, store to offset 16
        (v128.store (i32.const 16) (v128.load (i32.const 0)))
        ;; Verify by extracting lane 0 from reloaded value
        (i32x4.extract_lane 0 (v128.load (i32.const 16)))
    )

    ;; v128.load8_splat
    (func (export "test_load8_splat") (result i32)
        ;; Load byte at offset 4 (value 0x05) and splat
        (i8x16.extract_lane_u 7 (v128.load8_splat (i32.const 4)))
    )

    ;; v128.load16_splat
    (func (export "test_load16_splat") (result i32)
        ;; Load halfword at offset 0 (value 0x0201) and splat
        (i16x8.extract_lane_u 3 (v128.load16_splat (i32.const 0)))
    )

    ;; v128.load32_splat
    (func (export "test_load32_splat") (result i32)
        ;; Load word at offset 0 (value 0x04030201) and splat
        (i32x4.extract_lane 2 (v128.load32_splat (i32.const 0)))
    )

    ;; v128.load64_splat
    (func (export "test_load64_splat") (result i64)
        ;; Load doubleword at offset 0 and splat
        (i64x2.extract_lane 1 (v128.load64_splat (i32.const 0)))
    )

    ;; v128.load8x8_s (sign extend 8->16)
    (func (export "test_load8x8s") (result i32)
        ;; Load 8 bytes from offset 0, sign-extend to i16x8
        (i16x8.extract_lane_u 0 (v128.load8x8_s (i32.const 0)))
    )

    ;; v128.load32_zero
    (func (export "test_load32_zero") (result i32)
        ;; Load 32-bit from offset 0, zero upper
        (i32x4.extract_lane 1 (v128.load32_zero (i32.const 0)))
    )

    ;; v128.load8_lane
    (func (export "test_load8_lane") (result i32)
        (v128.load8_lane 3 (i32.const 5) (v128.const i32x4 0 0 0 0))  ;; Load byte at addr 5 (value 0x06), replace lane 3
        (i8x16.extract_lane_u 3)
    )

    ;; v128.store8_lane
    (func (export "test_store8_lane") (result i32)
        ;; Store lane 2 of a known vector to memory address 31
        (v128.store8_lane 2 (i32.const 31) (v128.const i8x16 0x41 0x42 0x43 0x44 0 0 0 0 0 0 0 0 0 0 0 0))
        ;; Read back
        (i32.load8_u (i32.const 31))
    )

    ;; v128.load16_lane
    (func (export "test_load16_lane") (result i32)
        (v128.load16_lane 1 (i32.const 2) (v128.const i32x4 0 0 0 0))  ;; Load halfword at addr 2 (value 0x0403), replace lane 1
        (i16x8.extract_lane_u 1)
    )

    ;; v128.store16_lane
    (func (export "test_store16_lane") (result i32)
        (v128.store16_lane 1 (i32.const 30) (v128.const i16x8 0x1234 0x5678 0 0 0 0 0 0))
        (i32.load16_u (i32.const 30))
    )

    ;; v128.load32_lane
    (func (export "test_load32_lane") (result i32)
        (v128.load32_lane 2 (i32.const 0) (v128.const i32x4 0 0 0 0))  ;; Load word at addr 0 (0x04030201), replace lane 2
        (i32x4.extract_lane 2)
    )

    ;; v128.store32_lane
    (func (export "test_store32_lane") (result i32)
        (v128.store32_lane 0 (i32.const 28) (v128.const i32x4 0xDEADBEEF 0 0 0))
        (i32.load (i32.const 28))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const e = instance.exports

    // v128.load + v128.store: bytes 0-3 are 0x04030201 in little-endian
    assert.eq(e.test_load_store(), 0x04030201)

    // v128.load8_splat: byte at offset 4 = 0x05
    assert.eq(e.test_load8_splat(), 5)

    // v128.load16_splat: halfword at offset 0 = 0x0201
    assert.eq(e.test_load16_splat(), 0x0201)

    // v128.load32_splat: word at offset 0 = 0x04030201
    assert.eq(e.test_load32_splat(), 0x04030201)

    // v128.load64_splat: dword at offset 0 = 0x0807060504030201
    assert.eq(e.test_load64_splat(), 0x0807060504030201n)

    // v128.load8x8_s: byte 0x01 sign-extended to i16 = 1
    assert.eq(e.test_load8x8s(), 1)

    // v128.load32_zero: upper lane should be 0
    assert.eq(e.test_load32_zero(), 0)

    // v128.load8_lane: byte at addr 5 = 0x06
    assert.eq(e.test_load8_lane(), 6)

    // v128.store8_lane: lane 2 = 0x43
    assert.eq(e.test_store8_lane(), 0x43)

    // v128.load16_lane: halfword at addr 2 = 0x0403
    assert.eq(e.test_load16_lane(), 0x0403)

    // v128.store16_lane: lane 1 = 0x5678
    assert.eq(e.test_store16_lane(), 0x5678)

    // v128.load32_lane: word at addr 0 = 0x04030201
    assert.eq(e.test_load32_lane(), 0x04030201)

    // v128.store32_lane: lane 0 = 0xDEADBEEF
    assert.eq(e.test_store32_lane(), 0xDEADBEEF | 0)
}

await assert.asyncTest(test())
