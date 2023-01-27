//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

 let wat = `
 (module
    (memory 1)

    (func (export "shuffle_identity1") (param i32) (result i64 i64)
        (local $res v128)
        (i8x16.splat (local.get 0))
        (v128.const i8x16 11 12 0 3 0 40 0 2 12 10 0 0 33 0 32 4)
        (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) ;; identity for the first operand!
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "shuffle_identity2") (param i32) (result i64 i64)
        (local $res v128)
        (v128.const i8x16 11 12 0 3 0 40 0 2 12 10 0 0 33 0 32 4)
        (i8x16.splat (local.get 0))
        (i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31) ;; identity for the second operand!
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i8x16dup1") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i8x16dup2") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i16x8dup1") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 2 3 2 3 2 3 2 3 2 3 2 3 2 3 2 3)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i16x8dup2") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 18 19 18 19 18 19 18 19 18 19 18 19 18 19 18 19)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i16x8dup_wrong") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i32x4dup1") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 4 5 6 7 4 5 6 7 4 5 6 7 4 5 6 7)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i32x4dup2") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 16 17 18 19 16 17 18 19 16 17 18 19 16 17 18 19)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i32x4dup_wrong") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 2 3 4 5 2 3 4 5 2 3 4 5 2 3 4 5)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i64x2dup1") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i64x2dup2") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 24 25 26 27 28 29 30 31 24 25 26 27 28 29 30 31)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )

    (func (export "i64x2dup_wrong") (param i32 i32) (result i64 i64)
        (local $res v128)
        (i32x4.splat (local.get 0))
        (i32x4.splat (local.get 1))
        (i8x16.shuffle 2 3 4 5 6 7 8 9 2 3 4 5 6 7 8 9)
        (local.tee $res)
        (i64x2.extract_lane 0)
        (local.get $res)
        (i64x2.extract_lane 1)
        (return)
    )
)`

 async function test() {
    const instance = await instantiate(wat, { imports: { } }, { simd: true })

    const {
        shuffle_identity1,
        shuffle_identity2,
        i8x16dup1,
        i8x16dup2,
        i16x8dup1,
        i16x8dup2,
        i16x8dup_wrong,
        i32x4dup1,
        i32x4dup2,
        i32x4dup_wrong,
        i64x2dup1,
        i64x2dup2,
        i64x2dup_wrong,
    } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(shuffle_identity1(0x7f), [0x7f7f7f7f7f7f7f7fn, 0x7f7f7f7f7f7f7f7fn])
        assert.eq(shuffle_identity2(0x5e), [0x5e5e5e5e5e5e5e5en, 0x5e5e5e5e5e5e5e5en])

        assert.eq(i8x16dup1(0x01020304, 0x05060708), [0x0303030303030303n, 0x0303030303030303n])
        assert.eq(i8x16dup2(0x01020304, 0x05060708), [0x0707070707070707n, 0x0707070707070707n])

        assert.eq(i16x8dup1(0x01020304, 0x05060708), [0x0102010201020102n, 0x0102010201020102n])
        assert.eq(i16x8dup2(0x01020304, 0x05060708), [0x0506050605060506n, 0x0506050605060506n])
        assert.eq(i16x8dup_wrong(0x01020304, 0x05060708), [0x0203020302030203n, 0x0203020302030203n])

        assert.eq(i32x4dup1(0x01020304, 0x05060708), [0x0102030401020304n, 0x0102030401020304n])
        assert.eq(i32x4dup2(0x01020304, 0x05060708), [0x0506070805060708n, 0x0506070805060708n])
        assert.eq(i32x4dup_wrong(0x01020304, 0x05060708), [0x0304010203040102n, 0x0304010203040102n])

        assert.eq(i64x2dup1(0x01020304, 0x05060708), [0x0102030401020304n, 0x0102030401020304n])
        assert.eq(i64x2dup2(0x01020304, 0x05060708), [0x0506070805060708n, 0x0506070805060708n])
        assert.eq(i64x2dup_wrong(0x01020304, 0x05060708), [0x0304010203040102n, 0x0304010203040102n])
    }
 }

 assert.asyncTest(test())
