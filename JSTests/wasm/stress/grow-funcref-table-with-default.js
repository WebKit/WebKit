import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// JS-API WebAssembly.Table.prototype.grow with a non-null funcref default
// goes through Wasm::Table::grow with a real fill value. That path used to
// populate only m_value while leaving m_function default-constructed
// (Bug A: call_indirect would dereference null rtt; Bug B: isEmpty()
// reading m_function.rtt was true, so visitAggregate skipped marking the
// wrapper held in m_value).
//
// Wasm-side `(table.grow ...)` lowers to tableGrow + tableSet which
// populate the slot correctly, so this test must use the JS API to
// reach the buggy path.

let mainWat = `
(module
    (type $sig (func (param i32) (result i32)))
    (table $t (export "t") 1 funcref)
    (elem (i32.const 0) $base)
    (func $base (param i32) (result i32) (i32.add (local.get 0) (i32.const 100)))
    (func (export "callIndirect") (param i32 i32) (result i32)
        (call_indirect (type $sig) (local.get 1) (local.get 0)))
)
`

let helperWat = `
(module
    (func (export "f") (param i32) (result i32) (i32.add (local.get 0) (i32.const 7)))
)
`

async function testCallIndirectAfterGrow() {
    const main = (await instantiate(mainWat, {}, {})).exports
    const fn = (await instantiate(helperWat, {}, {})).exports.f

    const oldSize = main.t.grow(4, fn)
    assert.eq(oldSize, 1)
    assert.eq(main.t.length, 5)

    for (let i = 1; i < 5; ++i)
        assert.eq(main.callIndirect(i, 10), 17)

    assert.eq(main.callIndirect(0, 10), 110)
}

async function testGCPreservesWrapperAfterGrow() {
    const main = (await instantiate(mainWat, {}, {})).exports

    // Many grows, each with an ephemeral wrapper from a fresh instance.
    // After the loop, the only path to each wrapper is the slot's m_value;
    // with the marking gap, fullGC reclaims them and table.get / call return
    // dangling cells.
    const N = 200;
    for (let i = 0; i < N; ++i) {
        const fn = (await instantiate(helperWat, {}, {})).exports.f
        main.t.grow(1, fn)
    }

    fullGC()

    for (let i = 1; i <= N; ++i) {
        const f = main.t.get(i)
        assert.eq(typeof f, "function")
        assert.eq(f(5), 12)
        assert.eq(main.callIndirect(i, 5), 12)
    }
}

async function testNullDefault() {
    const main = (await instantiate(mainWat, {}, {})).exports

    const oldSize = main.t.grow(3, null)
    assert.eq(oldSize, 1)
    assert.eq(main.t.get(1), null)
    assert.eq(main.t.get(2), null)

    let threw = false
    try {
        main.callIndirect(2, 0)
    } catch (e) {
        threw = e instanceof WebAssembly.RuntimeError
    }
    assert.eq(threw, true)
}

async function test() {
    await testCallIndirectAfterGrow()
    await testGCPreservesWrapperAfterGrow()
    await testNullDefault()
}

await assert.asyncTest(test())
