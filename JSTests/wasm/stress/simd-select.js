//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test_lower") (param i32 i32) (result i32) (local v128 v128)
        v128.const i32x4 1 1 1 1
        local.set 2
        v128.const i32x4 2 2 2 2
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
        v128.const i32x4 1 1 1 1
        local.set 2
        v128.const i32x4 2 2 2 2
        local.set 3

        local.get 2
        local.get 3
        local.get 0
        local.get 1
        i32.eq
        select

        i32x4.extract_lane 3
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { test_lower, test_upper } = instance.exports;
    assert.eq(test_lower(42, 42), 1);
    assert.eq(test_lower(42, 43), 2);
    assert.eq(test_upper(42, 42), 1);
    assert.eq(test_upper(42, 43), 2);
}

assert.asyncTest(test())
