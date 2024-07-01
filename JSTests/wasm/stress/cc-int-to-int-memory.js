//@ skip
//@ requireOptions("--useJITCage=0") # temporary workaround for rdar://127308350
//@ requireOptions("--useInterpretedJSEntryWrappers=1")
//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useInterpretedJSEntryWrappers=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory 1)

    (data (i32.const 0) "\\2A")

    (func $test (export "test") (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.load (i32.const 0)))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports

    for (let i = 0; i < 10000000; ++i) {
        assert.eq(test(5), 42 + 5)
        assert.eq(test(), 42 + 0)
        assert.eq(test(null), 42 + 0)
        assert.eq(test({ }), 42 + 0)
        assert.eq(test({ }, 10), 42 + 0)
        assert.eq(test(20.1, 10), 42 + 20)
        assert.eq(test(10, 20.1), 42 + 10)
    }
}

await assert.asyncTest(test())
