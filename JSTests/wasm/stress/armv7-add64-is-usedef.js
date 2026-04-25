import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test") (param $x i64) (param $y i64) (result i64)
        (i64.add (i64.const 1)
                (i64.add
                    (i64.xor (local.get $y) (local.get $x))
                    (i64.or (call $fun (local.get $y)) (i64.const 1))
                )
        )
)

    (func $fun (param $x i64) (result i64)
        (local.get $x)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let a = (1n<<32n) - 1n
        let b = (1n<<40n) + (1n<<33n) + (1n<<32n) + (1n<<31n)
        let c = 1n
        assert.eq(test(a, b), c + (b | 1n) + (b ^ a))
    }
}

await assert.asyncTest(test())
