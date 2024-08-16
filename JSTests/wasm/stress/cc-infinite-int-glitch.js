//@ requireOptions("--useWasmJITLessJSEntrypoint=1")
//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useWasmJITLessJSEntrypoint=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test") (param $x i32) (result i32) (result i32)
        (local.get $x)
        (local.get $x)
    )
    (func $test2 (export "test2") (param $x i32) (result i32) (result i32) (result i32)
        (local.get $x)
        (local.get $x)
        (local.get $x)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test2 } = instance.exports

    assert.eq(test(5), [5, 5])
    assert.eq(test2(5), [5, 5, 5])
}

await assert.asyncTest(test())
