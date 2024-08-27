//@ requireOptions("--useWasmJITLessJSEntrypoint=1")
//  Debugging: jsc -m cc-int-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useWasmJITLessJSEntrypoint=1 --dumpDisassembly=0
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

let wat2 = `
(module
    (type $sig_test (func (param i32) (result i32)))
    (import "o" "tbl" (table $t 1 funcref))
    (import "o" "test" (func $test (param $x i32) (result i32)))
    (import "o" "callee" (func $callee (param $x i32) (result i32)))

    (memory 1)

    (data (i32.const 0) "\\3B")

    (func $test2 (export "test2") (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.load (i32.const 0)))
    )

    (func $test3 (export "test3") (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.load (i32.const 0)))
    )

    (func (export "test_with_call") (param $x i32) (result i32)
        (i32.load (i32.const 0))
        (call $test (i32.const 1))
        (i32.load (i32.const 0))
        (i32.add)
        (i32.add)
    )

    (func (export "test_with_call_to_js") (param $x i32) (result i32)
        (i32.load (i32.const 0))
        (call $test (i32.const 1))
        (i32.load (i32.const 0))
        (i32.const 5)
        (call $callee)
        (i32.load (i32.const 0))
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports
    let tbl = new WebAssembly.Table({ initial: 1, element: "funcref" })
    tbl.set(0, test)
    function callee(x) {
        return x + test();
    }
    const instance2 = await instantiate(wat2, { o: { test, tbl, callee } }, { simd: true })
    const { test2, test3, test_with_call, test_with_call_to_js } = instance2.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(5), 42 + 5)
        assert.eq(test(), 42 + 0)
        assert.eq(test(null), 42 + 0)
        assert.eq(test({ }), 42 + 0)
        assert.eq(test({ }, 10), 42 + 0)
        assert.eq(test(20.1, 10), 42 + 20)
        assert.eq(test(10, 20.1), 42 + 10)

        assert.eq(test2(5), 59 + 5)
        assert.eq(test_with_call(1), 59 + 59 + 42 + 1)
        assert.eq(test_with_call_to_js(1), 59 + 59 + 59 + 42 + 1 + 5 + 42)
    }
}

await assert.asyncTest(test())
