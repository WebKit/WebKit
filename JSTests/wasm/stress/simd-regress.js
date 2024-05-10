//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $main (export "main") (result i32)
        v128.const i64x2 -27866447905751188 -27866447902605412
        v128.const i32x4 0x00080008 0x00080008 0x00080008 0x00080008
        i32x4.dot_i16x8_s
        i64x2.extract_lane 0
        i64.const -6867652708672
        i64.eq
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { main } = instance.exports

    for (let i = 0; i < 1000; ++i) {
        assert.eq(main(), 1)
    }
}

await assert.asyncTest(test())
