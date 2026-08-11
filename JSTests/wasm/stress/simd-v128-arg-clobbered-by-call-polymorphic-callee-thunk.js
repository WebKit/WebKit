//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

/*
Regression test for an ARM64 wasm thunk bug where callPolymorphicCalleeGenerator
(the call_indirect IC-miss slow path) saves/restores v0-v7 at Width64 instead
of Width128. Because the thunk tail-calls the resolved callee via brab, the
upper 64 bits of any v128 argument passed in v0-v7 are clobbered by the C
helper.

We trigger the slow path by rotating through more callees than the 4-way IC
holds. Each callee is `(param v128) (result v128)` and returns its argument
unchanged. The driver passes a v128 whose high lane is a known nonzero
pattern, then reads lane 1 back; if the bug strikes, the high lane comes back
as 0 (or garbage).
*/

let wat = `
(module
    (type $sig (func (param v128) (result v128)))

    (table 6 6 funcref)

    (func $f0 (type $sig) (local.get 0))
    (func $f1 (type $sig) (local.get 0))
    (func $f2 (type $sig) (local.get 0))
    (func $f3 (type $sig) (local.get 0))
    (func $f4 (type $sig) (local.get 0))
    (func $f5 (type $sig) (local.get 0))

    (elem (i32.const 0) $f0 $f1 $f2 $f3 $f4 $f5)

    ;; Takes an index, does call_indirect with a v128 whose high lane is
    ;; 0xDEADBEEFDEADBEEF, and returns the high lane of the result.
    (func $driver (export "driver") (param $idx i32) (result i64)
        (i64x2.extract_lane 1
            (call_indirect (type $sig)
                (v128.const i64x2 0xCAFEBABE00000000 0xDEADBEEFDEADBEEF)
                (local.get $idx))))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { driver } = instance.exports

    // i64x2.extract_lane returns a signed i64 BigInt; mask to unsigned for bit-pattern compare.
    const expected = 0xDEADBEEFDEADBEEFn

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        const idx = i % 6
        const got = BigInt.asUintN(64, driver(idx))
        if (got !== expected) {
            print("FAIL at iteration " + i + " idx=" + idx +
                  " expected=0x" + expected.toString(16) +
                  " got=0x" + got.toString(16))
            throw new Error("v128 high lane clobbered by call_indirect thunk")
        }
    }
}

await assert.asyncTest(test())
