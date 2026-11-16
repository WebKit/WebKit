//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test_lower") (param i32 i32) (result i32) (local v128 v128)
        v128.const i32x4 1 2 3 4
        local.set 2
        v128.const i32x4 5 6 7 8
        local.set 3

        local.get 2
        local.get 3
        local.get 0
        local.get 1
        i32.eq
        select

        i32x4.extract_lane 0
    )

    (func (export "test_upper") (param i32 i32) (result i32) (local v128 v128)
        v128.const i32x4 1 2 3 4
        local.set 2
        v128.const i32x4 5 6 7 8
        local.set 3

        local.get 2
        local.get 3
        local.get 0
        local.get 1
        i32.eq
        select

        i32x4.extract_lane 3
    )

    (func (export "test_select_t_lower") (param i32 i32) (result i32) (local v128 v128)
        v128.const i32x4 10 20 30 40
        local.set 2
        v128.const i32x4 50 60 70 80
        local.set 3

        local.get 2
        local.get 3
        local.get 0
        local.get 1
        i32.eq
        select (result v128)

        i32x4.extract_lane 0
    )

    (func (export "test_select_t_upper") (param i32 i32) (result i32) (local v128 v128)
        v128.const i32x4 10 20 30 40
        local.set 2
        v128.const i32x4 50 60 70 80
        local.set 3

        local.get 2
        local.get 3
        local.get 0
        local.get 1
        i32.eq
        select (result v128)

        i32x4.extract_lane 3
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { test_lower, test_upper, test_select_t_lower, test_select_t_upper } = instance.exports;
    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(test_lower(42, 42), 1);
        assert.eq(test_lower(42, 43), 5);
        assert.eq(test_upper(42, 42), 4);
        assert.eq(test_upper(42, 43), 8);
        assert.eq(test_select_t_lower(42, 42), 10);
        assert.eq(test_select_t_lower(42, 43), 50);
        assert.eq(test_select_t_upper(42, 42), 40);
        assert.eq(test_select_t_upper(42, 43), 80);
    }
}

await assert.asyncTest(test())
