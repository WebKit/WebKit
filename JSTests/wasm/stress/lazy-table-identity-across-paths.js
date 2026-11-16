import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// All three ways of getting a funcref for the same internal function should
// produce the same function object:
//   1. the exports object
//   2. ref.func from wasm
//   3. table.get from JS

let wat = `
(module
    (table $t (export "t") 1 1 funcref)
    (elem (i32.const 0) $foo)

    (func $foo (export "foo") (param i32) (result i32)
        (i32.add (local.get 0) (i32.const 100)))

    (func (export "refToFoo") (result funcref)
        (ref.func $foo))
)
`

async function test() {
    const { t, foo, refToFoo } = (await instantiate(wat, {}, {})).exports

    assert.eq(foo(1), 101)

    const fromRefFunc = refToFoo()
    assert.eq(fromRefFunc === foo, true)

    const fromTable = t.get(0)
    assert.eq(fromTable === foo, true)
    assert.eq(fromTable === fromRefFunc, true)

    // Identity holds across GC.
    fullGC()
    assert.eq(t.get(0) === foo, true)
    assert.eq(refToFoo() === foo, true)
    assert.eq(t.get(0)(5), 105)
}

await assert.asyncTest(test())
