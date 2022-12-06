//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $js_ident (import "imports" "ident") (param i32) (result i32))
    (func (export "test") (param $sz i32) (result i32)
        (local $a v128)
        (local $b v128)
        (local.set $a (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))
        (local.set $b (v128.const i8x16 9 0 2 0 0 0 0 0 0 0 5 0 0 0 0 1))
        (i8x16.extract_lane_u 15 (local.get $a))
        (i8x16.extract_lane_u 15 (local.get $b))
        (i32.add)

        (return)
    )
    (func $ident128 (param $i i32) (param $v v128) (param $i2 i32) (result v128)
        (return (local.get $v))
    )
    (func $ident32 (param $i i32) (param $v v128) (param $i2 i32) (result i32)
        (return (local.get $i2))
    )
    (func (export "test_spill") (param $i i32) (result i32)
        (local $a v128)
        (local.set $a (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))
        (i8x16.extract_lane_u 13
            (call $ident128
                (i32.const 1337)
                (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
                (i32.const 4200000)))
        (local.get $i)
        (i8x16.extract_lane_u 14 (local.get $a))
        (call $ident32
            (i32.const 1337)
            (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
            (i32.const 4200000))
        (i32.add)
        (i32.add)
        (i32.add)

        (return)
    )
    (func (export "test_spill_jscall") (param $i i32) (result i32)
        (local $a v128)
        (local.set $a (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))
        (i8x16.extract_lane_u 13
            (call $ident128
                (i32.const 1337)
                (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
                (i32.const 4200000)))
        (local.get $i)
        (local.get $a)
        (call $js_ident (i32.const 0))
        (drop)
        (i8x16.extract_lane_u 14)
        (call $ident32
            (i32.const 1337)
            (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
            (i32.const 4200000))
        (i32.add)
        (i32.add)
        (i32.add)

        (return)
    )
    (func (export "test_if_i32") (param $i i32) (result i32)
        (local $a v128)
        (local.set $a (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))
        (local.get $i)
        (if (result i32)
            (then
                (i8x16.extract_lane_u 13
                    (call $ident128
                        (i32.const 1337)
                        (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
                        (i32.const 4200000))))
            (else
                (i8x16.extract_lane_u 14 (local.get $a))))
        (call $ident32
            (i32.const 1337)
            (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
            (i32.const 4200000))
        (i32.add)

        (return)
    )
    (func (export "test_if_v128") (param $i i32) (result i32)
        (local $a v128)
        (local.set $a (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))
        (local.get $i)
        (if (result v128)
            (then
                (call $ident128
                    (i32.const 1337)
                    (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
                    (i32.const 4200000)))
            (else
                (local.get $a)))
        (i8x16.extract_lane_u 14)
        (call $ident32
            (i32.const 1337)
            (v128.const i8x16 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160)
            (i32.const 4200000))
        (i32.add)

        (return)
    )
)`

async function test() {
    const instance = await instantiate(wat, { imports: { ident: (x) => x } }, { simd: true })
    const { test, test_spill, test_spill_jscall, test_if_i32, test_if_v128 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), 16+1)
        assert.eq(test_spill(5000), 140+5000+15+4200000)
        assert.eq(test_spill_jscall(5000), 140+5000+15+4200000)
        assert.eq(test_if_i32(0), 15+4200000)
        assert.eq(test_if_i32(1), 140+4200000)
        assert.eq(test_if_v128(0), 15+4200000)
        assert.eq(test_if_v128(1), 150+4200000)
    }
}

assert.asyncTest(test())