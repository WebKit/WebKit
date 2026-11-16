//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const test_i8x16_const = "(v128.const i8x16 0x00 0x01 0x7F 0x80 0xFF 0x81 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B)";
const test_i16x8_const = "(v128.const i16x8 0x0000 0x0001 0x7FFF 0x8000 0xFFFF 0x8001 0x0002 0x0003)";
const test_i32x4_const = "(v128.const i32x4 0x12345678 0x9ABCDEF0 0x11111111 0x22222222)";
const test_i64x2_const = "(v128.const i64x2 0x123456789ABCDEF0 0xFEDCBA0987654321)";
const test_f32x4_const = "(v128.const f32x4 1.5 -2.25 3.75 -4.125)";
const test_f64x2_const = "(v128.const f64x2 1.25 -3.5)";

let wat = `
(module
    ;; Test i8x16.extract_lane_s (signed byte extraction) - individual functions for each lane
    (func (export "test_i8x16_extract_lane_s_0") (result i32)
        (i8x16.extract_lane_s 0 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_s_1") (result i32)
        (i8x16.extract_lane_s 1 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_s_2") (result i32)
        (i8x16.extract_lane_s 2 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_s_3") (result i32)
        (i8x16.extract_lane_s 3 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_s_4") (result i32)
        (i8x16.extract_lane_s 4 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_s_5") (result i32)
        (i8x16.extract_lane_s 5 ${test_i8x16_const})
    )

    ;; Test i8x16.extract_lane_u (unsigned byte extraction) - key test cases
    (func (export "test_i8x16_extract_lane_u_0") (result i32)
        (i8x16.extract_lane_u 0 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_u_3") (result i32)
        (i8x16.extract_lane_u 3 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_u_4") (result i32)
        (i8x16.extract_lane_u 4 ${test_i8x16_const})
    )
    (func (export "test_i8x16_extract_lane_u_5") (result i32)
        (i8x16.extract_lane_u 5 ${test_i8x16_const})
    )

    ;; Test i16x8.extract_lane_s (signed 16-bit extraction)
    (func (export "test_i16x8_extract_lane_s_0") (result i32)
        (i16x8.extract_lane_s 0 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_s_2") (result i32)
        (i16x8.extract_lane_s 2 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_s_3") (result i32)
        (i16x8.extract_lane_s 3 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_s_4") (result i32)
        (i16x8.extract_lane_s 4 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_s_5") (result i32)
        (i16x8.extract_lane_s 5 ${test_i16x8_const})
    )

    ;; Test i16x8.extract_lane_u (unsigned 16-bit extraction)
    (func (export "test_i16x8_extract_lane_u_0") (result i32)
        (i16x8.extract_lane_u 0 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_u_3") (result i32)
        (i16x8.extract_lane_u 3 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_u_4") (result i32)
        (i16x8.extract_lane_u 4 ${test_i16x8_const})
    )
    (func (export "test_i16x8_extract_lane_u_5") (result i32)
        (i16x8.extract_lane_u 5 ${test_i16x8_const})
    )

    ;; Test i32x4.extract_lane
    (func (export "test_i32x4_extract_lane_0") (result i32)
        (i32x4.extract_lane 0 ${test_i32x4_const})
    )
    (func (export "test_i32x4_extract_lane_1") (result i32)
        (i32x4.extract_lane 1 ${test_i32x4_const})
    )

    ;; Test i64x2.extract_lane
    (func (export "test_i64x2_extract_lane_0") (result i64)
        (i64x2.extract_lane 0 ${test_i64x2_const})
    )
    (func (export "test_i64x2_extract_lane_1") (result i64)
        (i64x2.extract_lane 1 ${test_i64x2_const})
    )

    ;; Test f32x4.extract_lane
    (func (export "test_f32x4_extract_lane_0") (result f32)
        (f32x4.extract_lane 0 ${test_f32x4_const})
    )
    (func (export "test_f32x4_extract_lane_1") (result f32)
        (f32x4.extract_lane 1 ${test_f32x4_const})
    )
    (func (export "test_f32x4_extract_lane_2") (result f32)
        (f32x4.extract_lane 2 ${test_f32x4_const})
    )
    (func (export "test_f32x4_extract_lane_3") (result f32)
        (f32x4.extract_lane 3 ${test_f32x4_const})
    )

    ;; Test f64x2.extract_lane
    (func (export "test_f64x2_extract_lane_0") (result f64)
        (f64x2.extract_lane 0 ${test_f64x2_const})
    )
    (func (export "test_f64x2_extract_lane_1") (result f64)
        (f64x2.extract_lane 1 ${test_f64x2_const})
    )

    ;; Test v128.const basic functionality
    (func (export "test_v128_const") (result i32)
        (i8x16.extract_lane_u 0 (v128.const i8x16 0x42 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00))
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const {
        test_i8x16_extract_lane_s_0, test_i8x16_extract_lane_s_1, test_i8x16_extract_lane_s_2,
        test_i8x16_extract_lane_s_3, test_i8x16_extract_lane_s_4, test_i8x16_extract_lane_s_5,
        test_i8x16_extract_lane_u_0, test_i8x16_extract_lane_u_3, test_i8x16_extract_lane_u_4, test_i8x16_extract_lane_u_5,
        test_i16x8_extract_lane_s_0, test_i16x8_extract_lane_s_2, test_i16x8_extract_lane_s_3,
        test_i16x8_extract_lane_s_4, test_i16x8_extract_lane_s_5,
        test_i16x8_extract_lane_u_0, test_i16x8_extract_lane_u_3, test_i16x8_extract_lane_u_4, test_i16x8_extract_lane_u_5,
        test_i32x4_extract_lane_0, test_i32x4_extract_lane_1,
        test_i64x2_extract_lane_0, test_i64x2_extract_lane_1,
        test_f32x4_extract_lane_0, test_f32x4_extract_lane_1, test_f32x4_extract_lane_2, test_f32x4_extract_lane_3,
        test_f64x2_extract_lane_0, test_f64x2_extract_lane_1,
        test_v128_const
    } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(test_i8x16_extract_lane_s_0(), 0);      // 0x00 -> 0
        assert.eq(test_i8x16_extract_lane_s_1(), 1);      // 0x01 -> 1
        assert.eq(test_i8x16_extract_lane_s_2(), 127);    // 0x7F -> 127
        assert.eq(test_i8x16_extract_lane_s_3(), -128);   // 0x80 -> -128 (sign extended)
        assert.eq(test_i8x16_extract_lane_s_4(), -1);     // 0xFF -> -1 (sign extended)
        assert.eq(test_i8x16_extract_lane_s_5(), -127);   // 0x81 -> -127 (sign extended)

        assert.eq(test_i8x16_extract_lane_u_0(), 0);      // 0x00 -> 0
        assert.eq(test_i8x16_extract_lane_u_3(), 128);    // 0x80 -> 128 (unsigned)
        assert.eq(test_i8x16_extract_lane_u_4(), 255);    // 0xFF -> 255 (unsigned)
        assert.eq(test_i8x16_extract_lane_u_5(), 129);    // 0x81 -> 129 (unsigned)

        assert.eq(test_i16x8_extract_lane_s_0(), 0);        // 0x0000 -> 0
        assert.eq(test_i16x8_extract_lane_s_2(), 32767);    // 0x7FFF -> 32767
        assert.eq(test_i16x8_extract_lane_s_3(), -32768);   // 0x8000 -> -32768 (sign extended)
        assert.eq(test_i16x8_extract_lane_s_4(), -1);       // 0xFFFF -> -1 (sign extended)
        assert.eq(test_i16x8_extract_lane_s_5(), -32767);   // 0x8001 -> -32767 (sign extended)

        assert.eq(test_i16x8_extract_lane_u_0(), 0);        // 0x0000 -> 0
        assert.eq(test_i16x8_extract_lane_u_3(), 32768);    // 0x8000 -> 32768 (unsigned)
        assert.eq(test_i16x8_extract_lane_u_4(), 65535);    // 0xFFFF -> 65535 (unsigned)
        assert.eq(test_i16x8_extract_lane_u_5(), 32769);    // 0x8001 -> 32769 (unsigned)

        assert.eq(test_i32x4_extract_lane_0(), 0x12345678);
        assert.eq(test_i32x4_extract_lane_1(), 0x9ABCDEF0 | 0); // Force to signed 32-bit

        assert.eq(test_i64x2_extract_lane_0(), 0x123456789ABCDEF0n);
        assert.eq(test_i64x2_extract_lane_1(), -81986143110479071n); // 0xFEDCBA0987654321 as signed i64

        assert.eq(test_f32x4_extract_lane_0(), 1.5);
        assert.eq(test_f32x4_extract_lane_1(), -2.25);
        assert.eq(test_f32x4_extract_lane_2(), 3.75);
        assert.eq(test_f32x4_extract_lane_3(), -4.125);

        assert.eq(test_f64x2_extract_lane_0(), 1.25);
        assert.eq(test_f64x2_extract_lane_1(), -3.5);

        assert.eq(test_v128_const(), 0x42);
    }
}

await assert.asyncTest(test())
