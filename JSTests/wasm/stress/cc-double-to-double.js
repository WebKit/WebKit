//@ requireOptions("--useInterpretedJSEntryWrappers=1")
//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useInterpretedJSEntryWrappers=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $sig_test (func (param f64) (result f64)))
    (table $t 1 funcref)
    (elem (i32.const 0) $test)

    (func $test (export "test") (param $x f64) (result f64)
        (f64.add (local.get $x) (f64.const 42))
    )

    (func (export "test_with_call") (param $x f64) (result f64)
        (f64.add (local.get $x) (call $test (f64.const 1337)))
    )

    (func (export "test_with_call_indirect") (param $x f64) (result f64)
        (local.get $x)
        (f64.const 98)
        (call_indirect $t (type $sig_test) (i32.const 0))
        f64.add
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test_with_call, test_with_call_indirect } = instance.exports

    for (let i = 0; i < 1000; ++i) {
        assert.eq(test(5), 42 + 5)
        assert.eq(test(), NaN)
        assert.eq(test(null), 42 + 0)
        assert.eq(test({ }), NaN)
        assert.eq(test({ }, 10), NaN)
        assert.eq(test(20.1, 10), (42 + 20.1))
        assert.eq(test(10, 20.1), 42 + 10)
        assert.eq(test_with_call(5), 42 + 5 + 1337)
        assert.eq(test_with_call(), NaN)
        assert.eq(test_with_call(null), 42 + 0 + 1337)
        assert.eq(test_with_call({ }), NaN)
        assert.eq(test_with_call({ }, 10), NaN)
        assert.eq(test_with_call(20.1, 10), (42 + 20.1 + 1337))
        assert.eq(test_with_call(10, 20.1), 42 + 10 + 1337)
        assert.eq(test_with_call_indirect(5), 42 + 5 + 98)
        assert.eq(test_with_call_indirect(), NaN)
        assert.eq(test_with_call_indirect(null), 42 + 0 + 98)
        assert.eq(test_with_call_indirect({ }), NaN)
        assert.eq(test_with_call_indirect({ }, 10), NaN)
        assert.eq(test_with_call_indirect(20.1, 10), (42 + 20.1 + 98))
        assert.eq(test_with_call_indirect(10, 20.1), 42 + 10 + 98)
    }
}

await assert.asyncTest(test())
