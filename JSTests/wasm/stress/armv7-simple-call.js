import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test") (param $x i64) (param $y i64) (result i64)
        (i64.add (local.get $x)
            (i64.add (i64.const 1
                (i64.add
                    (i64.shl (local.get $y) (i64.const 1))
                    (call $fun (local.get $y)))))))

    (func $fun (param $x i64) (result i64)
        (i64.shr_u (local.get $x) (i64.const 1))
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
        assert.eq(test(a, b), a + (b >> 1n) + (b << 1n) + c)
    }
}

await assert.asyncTest(test())
