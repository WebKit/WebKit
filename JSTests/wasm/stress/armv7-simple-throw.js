import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (tag $e (param i64))

    (func $test (export "test") (param $x i64) (result i64)
        (try (result i64)
            (do
                (call $fun (i64.or (i64.const 0x00EFBA00FE0000AD (local.get $x))))
            )
            (catch $e
                return
            )
        )
    )

    (func $fun (param $x i64) (result i64)
        (if (result i64)
            (i64.eq (local.get $x) (i64.const 0x7EEFBAADFEEDDEAD))
            (then (throw $e (i64.const 0x7AADDAADDEEDFEED)))
            (else (i64.const 0x7DDDBAADDEADFAAD))
        )
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, exceptions: true })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let a = 0x7E0000AD00EDDE00n
        let b = 0x1234567890123456n
        let c = 0x7AADDAADDEEDFEEDn
        let d = 0x7DDDBAADDEADFAADn
        assert.eq(test(a), c)
        assert.eq(test(b), d)
    }
}

await assert.asyncTest(test())
