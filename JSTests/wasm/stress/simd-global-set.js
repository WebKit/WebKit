//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (global $0 (mut v128) v128.const i32x4 0 0 0 0)
    (func $1 (result v128)
        v128.const i32x4 1 2 3 4
        global.set $0
        global.get $0
    )
    (func (export "test")
        call $1
        v128.const i32x4 1 2 3 4
        i32x4.eq
        i32x4.all_true
        br_if 0
        unreachable
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { test } = instance.exports;
    test();
}

await assert.asyncTest(test())
