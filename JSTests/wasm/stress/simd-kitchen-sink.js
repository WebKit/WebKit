//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

 let wat = `
 (module
    (global $g (mut v128) (v128.const f32x4 0.0 0.0 0.0 0.0))

    (memory 1)
    (global $zero (mut v128) (v128.const i32x4 0 0 0 0))

    (func (export "bitmask") (result i32)
        (v128.const i64x2 13 -5)
        (i64x2.bitmask)

        (return)
    )

    (func (export "add") (result i64)
        (v128.const i64x2 5 10)
        (v128.const i64x2 15 20)
        (i64x2.add)
        (i64x2.extract_lane 1)

        (return)
    )

    (func (export "sub") (result i64)
        (i64x2.sub (v128.const i64x2 15 20) (v128.const i64x2 5 10))
        (i64x2.extract_lane 1)

        (return)
    )

    (func (export "splat") (result i64)
        (i64x2.splat (i64.const 5))
        (i64x2.extract_lane 1)

        (return)
    )

    (func (export "shl") (result i64)
        (i64x2.shl (v128.const i64x2 7 90) (i32.const 1))
        (i64x2.extract_lane 0)

        (return)
    )

    (func (export "extmul") (result i64)
        (i64x2.extmul_high_i32x4_u (v128.const i32x4 -1 -1 5 10) (v128.const i32x4 -1 -1 10 100))
        (i64x2.extract_lane 1)

        (return)
    )

    (func (export "shuffle") (result i64)
        (v128.const i8x16 42 10 0 0 0 0 0 2 0 0 0 0 0 0 0 4)
        (v128.const i8x16 99 5 0 0 0 0 0 1 0 0 0 0 0 0 0 3)
        (i8x16.shuffle 0 1 16 17 0 0 0 1 2 2 2 2 1 1 1 1)
        (i64x2.extract_lane 0)

        (return)
    )

    (func (export "bitwise_select") (result i64)
        (v128.bitselect (v128.const i64x2 0 4294967295) (v128.const i64x2 0 0) (v128.const i64x2 0 7))
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "eq") (result i64)
        (i64x2.eq (v128.const i64x2 10 20) (v128.const i64x2 40 20))
        (i64x2.extract_lane 1)

        (return)
    )

    (func (export "smax") (result i32)
        (i32x4.max_s (v128.const i32x4 -1 1 0 0) (v128.const i32x4 2 0 0 0))
        (i32x4.extract_lane 0)

        (return)
    )

    (func (export "subsat") (result i32)
        (i16x8.sub_sat_s (v128.const i16x8 -1 -1 10 20 0 0 0 1) (v128.const i16x8 2 -2 5 25 0 0 0 2))
        (i32x4.extract_lane 0)

        (return)
    )

    (func (export "q15mulr_sat_s") (result i32)
        (i16x8.q15mulr_sat_s (v128.const i16x8 8192 0 0 0 0 0 0 0) (v128.const i16x8 24576 0 0 0 0 0 0 0)) ;; 1/4 * 3/4 = 3/16 = 6144_q15
        (i16x8.extract_lane_s 0)

        (return)
    )

    (func (export "not") (result i32)
        (v128.not (v128.const i32x4 0 0 0 0))
        (i32x4.eq (v128.const i32x4 -1 -1 -1 -1))
        (i32x4.extract_lane 0)

        (return)
    )

    (func (export "as_i8x16_extract_lane_s_operand_last") (param i32) (result i32)
        (i8x16.extract_lane_s 15 (i8x16.splat (local.get 0))))

    (func (export "as_i16x8_add_sub_mul_operands") (param i32 i32 i32 i32) (result i32)
        (local.set 0 (i32.const 257))
        (local.set 1 (i32.const 128))
        (local.set 2 (i32.const 16))
        (local.set 3 (i32.const 16))

        (i16x8.mul (v128.const i16x8 16 16 16 16 16 16 16 16) (v128.const i16x8 16 16 16 16 16 16 16 16))
        (i32x4.extract_lane 0))

    (func (export "globals") (param $i i32) (result i32)
        (global.set $g (i32x4.splat (local.get $i)))
        (i32x4.extract_lane 0 (global.get $g))

        (return))

    (func (export "store8")
        (param $address i32) (result i64) (local $ret i64)
        
        (v128.store8_lane align=1 4 (local.get $address) (v128.const i8x16 0 0 0 0 4 0 0 0 0 0 0 0 0 0 0 0))
        (local.set $ret (i64.load (local.get $address)))
        (v128.store offset=4 (i32.const 0) (global.get $zero))
        (local.get $ret))
    
    (func (export "trunc") (result i32)
        (v128.const f64x2 -2147483647.0 -2147483647.0)
        (i32x4.trunc_sat_f64x2_s_zero)
        (i32x4.extract_lane 0))

    (func (export "bitmask8") (result i32)
        (i8x16.bitmask (v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)))
    (func (export "bitmask8_2") (result i32)
        (i8x16.bitmask (v128.const i8x16 -1 0 1 2 3 4 5 6 7 8 9 0xA 0xB 0xC 0xD 0xF)))

    (func (export "convert_low") (result f64)
        (f64x2.extract_lane 0 (f64x2.convert_low_i32x4_s (v128.const i32x4 2147483647 2147483647 0 0))))

    (func (export "pmin") (result f32)
        (f32x4.extract_lane 0 (f32x4.pmin
            (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)
            (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))))
 )
 `

 async function test() {
    const instance = await instantiate(wat, { imports: { } }, { simd: true })

    const {
        bitmask,
        add,
        sub,
        splat,
        shl,
        extmul,
        shuffle,
        bitwise_select,
        eq,
        smax,
        subsat,
        q15mulr_sat_s,
        not,
        as_i8x16_extract_lane_s_operand_last,
        as_i16x8_add_sub_mul_operands,
        globals,
        store8,
        trunc,
        bitmask8,
        bitmask8_2,
        convert_low,
        pmin,
    } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(bitmask(), 0b010)
        assert.eq(add(), 30n)
        assert.eq(sub(), 10n)
        assert.eq(splat(), 5n)
        assert.eq(shl(), 14n)
        assert.eq(extmul(), 1000n)
        assert.eq(shuffle(), 732444249368496682n)
        assert.eq(bitwise_select(), 7n)
        assert.eq(eq(), -1n)
        assert.eq(smax(), 2)
        assert.eq(subsat(), 131069)
        assert.eq(q15mulr_sat_s(), 6144)
        assert.eq(not(), -1)
        assert.eq(as_i8x16_extract_lane_s_operand_last(-42), -42)
        assert.eq(as_i16x8_add_sub_mul_operands(), 16777472)
        assert.eq(globals(1337), 1337)
        assert.eq(store8(4), 4n)
        assert.eq(trunc(), -2147483647)
        assert.eq(bitmask8(), 0x0000FFFF)
        assert.eq(bitmask8_2(), 0x00000001)
        assert.eq(convert_low(), 2147483647.0)
        assert.eq(pmin(), 0.0)
    }
 }

 assert.asyncTest(test())