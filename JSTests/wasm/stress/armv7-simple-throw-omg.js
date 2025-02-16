import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (tag $e (param i32))

    (func $test (export "test")
        (try
            (do
                (throw $e (i32.const 0xDFEED))
            )
            (catch $e
                return
            )
        )
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, exceptions: true })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let a = 0x7E0000AD00EDDE00n
        test(a)
    }
}

await assert.asyncTest(test())
