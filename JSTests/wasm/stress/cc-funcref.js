//@ requireOptions("--useInterpretedJSEntryWrappers=1")
//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useInterpretedJSEntryWrappers=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test") (param $x funcref) (result funcref)
        (local.get $x)
    )
    (func $test2 (export "test2") (param $x funcref) (result funcref) (result funcref)
        (local.get $x)
        (local.get $x)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test2 } = instance.exports

    assert.eq(test(null), null)
    assert.eq(test2(null), [null, null])
}

await assert.asyncTest(test())
