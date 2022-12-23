//@ requireOptions("--useWebAssemblySIMD=1", "--useConcurrentJIT=0")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (result i32)
        (local $r v128)
        (local $i i32)
        (v128.const i32x4 42 42 42 42)

        (loop $loop
            local.get $i
            i32.const 1
            i32.add
            local.set $i

            (i32x4.add (local.get $r) (v128.const i32x4 1 2 3 4))
            local.set $r

            local.get $i
            i32.const 1000
            i32.lt_s
            br_if $loop)

        (i32x4.extract_lane 0)
        (i32x4.extract_lane 0 (local.get $r))
        i32.add
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(), 1000*1 + 42)
    }
}

assert.asyncTest(test())
