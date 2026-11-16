//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (import "m" "p" (func $p (param i64 i64 i64 i64 i64 i64 i64 i64) (result i64)))

    (func $test (export "test") (param i64 i64 i64 i64 i64 i64 i64 i64) (result i64)
        (call $p
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (local.get 3)
        (local.get 4)
        (local.get 5)
        (local.get 6)
        (local.get 7)
        )
    )
)
`

async function test() {
    const instance = await instantiate(wat, { m: { p: (...args) => args.reduce((x, y) => x+y, 0n) } }, { })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(test(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n), 36n)
    }
}

await assert.asyncTest(test())

