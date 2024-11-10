//@ requireOptions("--useOMGInlining=1", "--useWasmOSR=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (tag $e)

    (func $test (export "test") (param $a i32) (result i64)
        (local $x i64)
        (local.set $x (i64.add (i64.const 0x0EEFBAADDEADFEED) (i64.extend_i32_u (local.get $a))))
        (try
            (do
                (local.set $x (i64.add (i64.const 0x100000000) (local.get $x)))
                (call $fun (local.get $a))
                (local.set $x (i64.add (i64.const 1) (local.get $x)))
            )
            (catch $e
                (local.set $x (i64.add (i64.const 5) (local.get $x)))
            )
        )
        (try
            (do
                (local.set $x (i64.add (i64.const 0x100000000) (local.get $x)))
                (call $fun (local.get $a))
                (local.set $x (i64.add (i64.const 1) (local.get $x)))
            )
            (catch $e
                (local.set $x (i64.add (i64.const 5) (local.get $x)))
            )
        )
        (return (local.get $x))
    )

    (func $fun (param $x i32)
        (if
            (i32.eq (local.get $x) (i32.const 42))
            (then (throw $e))
        )
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, exceptions: true })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(41), 0x0EEFBAADDEADFEEDn + 41n + (0x100000000n + 1n) * 2n)
        assert.eq(test(42), 0x0EEFBAADDEADFEEDn + 42n + (0x100000000n + 5n) * 2n)
    }
}

await assert.asyncTest(test())
