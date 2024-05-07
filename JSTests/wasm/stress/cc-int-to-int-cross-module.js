//@ skip if $architecture != "arm64" and $architecture != "x86_64"
//@ requireOptions("--useIPIntWrappers=1")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test") (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.const 42))
    )
)
`

let wat2 = `
(module
    (type $sig_test (func (param i32) (result i32)))
    (import "o" "tbl" (table $t 1 funcref))
    (import "o" "test" (func $test (param $x i32) (result i32)))

    (func (export "test_with_call") (param $x i32) (result i32)
        (i32.add (local.get $x) (call $test (i32.const 1337)))
    )

    (func (export "test_with_call_indirect") (param $x i32) (result i32)
        (local.get $x)
        (i32.const 98)
        (call_indirect $t (type $sig_test) (i32.const 0))
        i32.add
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports
    let tbl = new WebAssembly.Table({ initial: 1, element: "funcref" })
    tbl.set(0, test)
    const instance2 = await instantiate(wat2, { o: { test, tbl } }, { simd: true })
    const { test_with_call, test_with_call_indirect } = instance2.exports

    for (let i = 0; i < 10000000; ++i) {
        assert.eq(test(5), 42 + 5)
        assert.eq(test(), 42 + 0)
        assert.eq(test(null), 42 + 0)
        assert.eq(test({ }), 42 + 0)
        assert.eq(test({ }, 10), 42 + 0)
        assert.eq(test(20.1, 10), 42 + 20)
        assert.eq(test(10, 20.1), 42 + 10)
        assert.eq(test_with_call(5), 42 + 5 + 1337)
        assert.eq(test_with_call(), 42 + 0 + 1337)
        assert.eq(test_with_call(null), 42 + 0 + 1337)
        assert.eq(test_with_call({ }), 42 + 0 + 1337)
        assert.eq(test_with_call({ }, 10), 42 + 0 + 1337)
        assert.eq(test_with_call(20.1, 10), 42 + 20 + 1337)
        assert.eq(test_with_call(10, 20.1), 42 + 10 + 1337)
        assert.eq(test_with_call_indirect(5), 42 + 5 + 98)
        assert.eq(test_with_call_indirect(), 42 + 0 + 98)
        assert.eq(test_with_call_indirect(null), 42 + 0 + 98)
        assert.eq(test_with_call_indirect({ }), 42 + 0 + 98)
        assert.eq(test_with_call_indirect({ }, 10), 42 + 0 + 98)
        assert.eq(test_with_call_indirect(20.1, 10), 42 + 20 + 98)
        assert.eq(test_with_call_indirect(10, 20.1), 42 + 10 + 98)
    }
}

await assert.asyncTest(test())
