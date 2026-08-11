import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

function checkAll(table, expected, label) {
    assert.eq(table.length, expected.length, label)
    for (let i = 0; i < table.length; ++i) {
        const v = table.get(i)
        const msg = `${label}[${i}]`
        if (expected[i] === null)
            assert.eq(v, null, msg)
        else if (expected[i] === undefined)
            assert.eq(v, undefined, msg)
        else
            assert.eq(v, expected[i], msg)
    }
}

const helperWat = `
(module
    (func (export "f") (result i32) (i32.const 42))
)
`

const callIndirectWat = `
(module
    (type $sig (func (result i32)))
    (import "env" "table" (table 4 funcref))
    (func (export "call") (param i32) (result i32)
        (call_indirect (type $sig) (local.get 0)))
)
`

const callIndirectGrowableWat = `
(module
    (type $sig (func (result i32)))
    (import "env" "table" (table 4 8 funcref))
    (func (export "call") (param i32) (result i32)
        (call_indirect (type $sig) (local.get 0)))
)
`

async function testDefaultsAndNull() {
    checkAll(new WebAssembly.Table({ element: "externref", initial: 4 }),
        [undefined, undefined, undefined, undefined], "externref default")
    checkAll(new WebAssembly.Table({ element: "externref", initial: 4 }, null),
        [null, null, null, null], "externref null")
    checkAll(new WebAssembly.Table({ element: "funcref", initial: 3 }),
        [null, null, null], "funcref default")
    checkAll(new WebAssembly.Table({ element: "funcref", initial: 3 }, null),
        [null, null, null], "funcref null")
}

async function testExternrefObjectFill() {
    const obj = { tag: "fill" }
    const t = new WebAssembly.Table({ element: "externref", initial: 5 }, obj)
    checkAll(t, [obj, obj, obj, obj, obj], "externref object")
    assert.eq(t.get(0), t.get(4))

    const size = 1000
    const large = new WebAssembly.Table({ element: "externref", initial: size }, obj)
    assert.eq(large.length, size)
    assert.eq(large.get(0), obj)
    assert.eq(large.get(size - 1), obj)
    assert.eq(large.get(size >> 1), obj)
}

async function testFuncrefFillAndCallIndirect() {
    const fn = (await instantiate(helperWat, {}, {})).exports.f

    const growable = new WebAssembly.Table({ element: "funcref", initial: 4, maximum: 8 }, fn)
    for (let i = 0; i < 4; ++i)
        assert.eq(growable.get(i), fn)
    assert.eq(growable.get(0)(), 42)

    const fixed = new WebAssembly.Table({ element: "funcref", initial: 4, maximum: 4 }, fn)
    for (let i = 0; i < 4; ++i)
        assert.eq(fixed.get(i), fn)
    assert.eq(fixed.get(1)(), 42)

    const callGrowable = (await instantiate(callIndirectGrowableWat, {
        env: { table: growable },
    }, {})).exports.call
    for (let i = 0; i < 4; ++i)
        assert.eq(callGrowable(i), 42)

    const callFixed = (await instantiate(callIndirectWat, {
        env: { table: fixed },
    }, {})).exports.call
    for (let i = 0; i < 4; ++i)
        assert.eq(callFixed(i), 42)
}

async function testFuncrefFillSurvivesGC() {
    const n = 64
    let growable
    let fixed
    let callFixed
    let callGrowable
    {
        const fn = (await instantiate(helperWat, {}, {})).exports.f
        growable = new WebAssembly.Table({ element: "funcref", initial: n, maximum: n * 2 }, fn)
        fixed = new WebAssembly.Table({ element: "funcref", initial: n, maximum: n }, fn)
        callGrowable = (await instantiate(`
(module
    (type $sig (func (result i32)))
    (import "env" "table" (table ${n} ${n * 2} funcref))
    (func (export "call") (param i32) (result i32)
        (call_indirect (type $sig) (local.get 0)))
)
`, { env: { table: growable } }, {})).exports.call
        callFixed = (await instantiate(`
(module
    (type $sig (func (result i32)))
    (import "env" "table" (table ${n} ${n} funcref))
    (func (export "call") (param i32) (result i32)
        (call_indirect (type $sig) (local.get 0)))
)
`, { env: { table: fixed } }, {})).exports.call
    }

    fullGC()

    for (let i = 0; i < n; ++i) {
        assert.eq(typeof growable.get(i), "function")
        assert.eq(growable.get(i)(), 42)
        assert.eq(callGrowable(i), 42)
        assert.eq(typeof fixed.get(i), "function")
        assert.eq(fixed.get(i)(), 42)
        assert.eq(callFixed(i), 42)
    }
}

async function testExternrefFillSurvivesGC() {
    const n = 100
    let table
    {
        table = new WebAssembly.Table({ element: "externref", initial: n }, { tag: 0xbeef })
    }
    fullGC()
    for (let i = 0; i < n; ++i) {
        const v = table.get(i)
        assert.eq(typeof v, "object")
        assert.eq(v.tag, 0xbeef)
    }
}

async function test() {
    await testDefaultsAndNull()
    await testExternrefObjectFill()
    await testFuncrefFillAndCallIndirect()
    await testFuncrefFillSurvivesGC()
    await testExternrefFillSurvivesGC()
}

await assert.asyncTest(test())
