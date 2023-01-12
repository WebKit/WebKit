//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param $sz i32) (result i64)
        (f32.const 1337)
        (f32.const 1)
        (f32.const 2)
        (f32.const 3)
        (f32.const 4)
        (f32.const 5)
        (f32.const 6)
        (f32.const 7)
        (f32.const 8)
        (f32.const 9)
        (v128.const f32x4 0 0 0 10)
        (return (call $f))
        (drop)
    )

    (func $f (param f32) (param f32) (param f32) (param f32) (param f32) (param f32) (param f32) (param f32) (param $i0 f32) (param $i1 f32) (param $i2 v128) (result i64)
        (i64.add
            (i64.trunc_f32_s (f32x4.extract_lane 3 (local.get $i2)))
            (i64.trunc_f32_s (local.get $i0)))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), 8n + 10n)
    }
}

await assert.asyncTest(test())
