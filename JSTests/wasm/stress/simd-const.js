//@ requireOptions("--useWebAssemblySIMD=1", "--useBBQJIT=1", "--webAssemblyBBQAirModeThreshold=0", "--wasmBBQUsesAir=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param $sz i32) (result i32)
        (i32.const 9)
        (drop)
        (return (i8x16.extract_lane_u 1 (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)))
    )

    (func (export "test2") (param $sz i32) (result i32)
        (return (i8x16.extract_lane_u 12 (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), 2)
        assert.eq(test2(48), 13);
    }
}

assert.asyncTest(test())
