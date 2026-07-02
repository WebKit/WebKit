import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (global $1 i64 i64.const 1)
    (global $2 i64 i64.const 2)
    (global $127 i64 i64.const 127)
    (global $i64-min i64 i64.const 0x8000000000000000)

    (func $shl (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.shl
    )
    (func $shr_s (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.shr_s
    )
    (func $shr_u (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.shr_u
    )
    (func $rotl (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.rotl
    )
    (func $rotr (param i64 i64) (result i64)
        local.get 0
        local.get 1
        i64.rotr
    )
    (func $assert_eq (param i64 i64)
        local.get 0
        local.get 1
        i64.eq
        br_if 0
        unreachable
    )
    (func (export "test_constant")
        global.get $1
        i64.const 64
        i64.shl
        i64.const 1
        call $assert_eq ;; 1 << 64 == 1

        global.get $1
        i64.const 65
        i64.shl
        i64.const 2
        call $assert_eq ;; 1 << 65 == 2

        global.get $127
        i64.const -128
        i64.shr_u
        i64.const 127
        call $assert_eq ;; 127u >> 128 == 127

        global.get $127
        i64.const 964 ;; 964 % 64 == 4
        i64.shr_u
        i64.const 7
        call $assert_eq ;; 127u >> 964 == 7

        global.get $127
        i64.const -256
        i64.shr_s
        i64.const 127
        call $assert_eq ;; 127 >> 256 == 127

        global.get $127
        i64.const 49095749 ;; 49095749 % 64 == 5
        i64.shr_s
        i64.const 3
        call $assert_eq ;; 127 >> 49095749 == 3

        global.get $i64-min
        i64.const 1024
        i64.shr_s
        global.get $i64-min
        call $assert_eq ;; INT64_MIN >> 1024 == INT64_MIN
        
        global.get $2
        i64.const 127
        i64.rotl
        i64.const 1
        call $assert_eq ;; 2 rotl 127 == 1
        
        global.get $2
        i64.const 127
        i64.rotr
        i64.const 4
        call $assert_eq ;; 2 rotr 127 == 4

        global.get $1
        i64.const 96
        i64.rotl
        global.get $1
        i64.const 96
        i64.rotr
        call $assert_eq ;; 1 rotl 96 == 1 rotr 96

        global.get $2
        i64.const 960
        i64.rotl
        global.get $2
        i64.const 960
        i64.rotr
        call $assert_eq ;; 2 rotl 960 == 2 rotr 960
    )
    (func (export "test_non_constant")
        global.get $1
        i64.const 64
        call $shl
        i64.const 1
        call $assert_eq ;; 1 << 64 == 1

        global.get $1
        i64.const 65
        call $shl
        i64.const 2
        call $assert_eq ;; 1 << 65 == 2

        global.get $127
        i64.const -128
        call $shr_u
        i64.const 127
        call $assert_eq ;; 127u >> 128 == 127

        global.get $127
        i64.const 964 ;; 964 % 64 == 4
        call $shr_u
        i64.const 7
        call $assert_eq ;; 127u >> 964 == 7

        global.get $127
        i64.const -256
        call $shr_s
        i64.const 127
        call $assert_eq ;; 127 >> 256 == 127

        global.get $127
        i64.const 49095749 ;; 49095749 % 64 == 5
        call $shr_s
        i64.const 3
        call $assert_eq ;; 127 >> 49095749 == 3

        global.get $i64-min
        i64.const 1024
        call $shr_s
        global.get $i64-min
        call $assert_eq ;; INT64_MIN >> 1024 == INT64_MIN
        
        global.get $2
        i64.const 127
        call $rotl
        i64.const 1
        call $assert_eq ;; 2 rotl 127 == 1
        
        global.get $2
        i64.const 127
        call $rotr
        i64.const 4
        call $assert_eq ;; 2 rotr 127 == 4

        global.get $1
        i64.const 96
        call $rotl
        global.get $1
        i64.const 96
        call $rotr
        call $assert_eq ;; 1 rotl 96 == 1 rotr 96

        global.get $2
        i64.const 960
        call $rotl
        global.get $2
        i64.const 960
        call $rotr
        call $assert_eq ;; 2 rotl 960 == 2 rotr 960
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { test_constant, test_non_constant } = instance.exports;
    test_constant();
    // test_non_constant();
}

await assert.asyncTest(test())
