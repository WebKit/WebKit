import { compile } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// When module A imports module B's table and re-exports it, JS observing the
// table through A must produce the same function objects as observing it
// through B, and calling those functions must execute B's code. call_indirect
// through the imported table must also work without ever observing the slot
// from JS.

let watB = `
(module
    (table $t (export "t") 2 2 funcref)
    (elem (i32.const 0) $foo $bar)

    (func $foo (export "foo") (param i32) (result i32)
        (i32.add (local.get 0) (i32.const 1000)))

    (func $bar (export "bar") (param i32) (result i32)
        (i32.add (local.get 0) (i32.const 2000)))
)
`

let watA = `
(module
    (type $sig (func (param i32) (result i32)))
    (import "B" "t" (table $t 2 2 funcref))
    (export "t" (table $t))

    (func (export "callViaImportedTable") (param i32 i32) (result i32)
        (call_indirect (type $sig) (local.get 1) (local.get 0)))
)
`

async function test() {
    const moduleB = await compile(watB)
    const moduleA = await compile(watA)

    const instB = new WebAssembly.Instance(moduleB)
    const instA = new WebAssembly.Instance(moduleA, { B: { t: instB.exports.t } })

    // call_indirect through the imported table on never-observed slots.
    assert.eq(instA.exports.callViaImportedTable(0, 5), 1005)
    assert.eq(instA.exports.callViaImportedTable(1, 5), 2005)

    // Re-exported table aliases B's table.
    assert.eq(instA.exports.t, instB.exports.t)

    // Reading from A's exported table returns B's exported function objects,
    // and calling them executes B's code.
    const fooFromA = instA.exports.t.get(0)
    assert.eq(fooFromA === instB.exports.foo, true)
    assert.eq(fooFromA(7), 1007)

    const barFromA = instA.exports.t.get(1)
    assert.eq(barFromA === instB.exports.bar, true)
    assert.eq(barFromA(7), 2007)

    // call_indirect continues to work after JS has observed the slots.
    assert.eq(instA.exports.callViaImportedTable(0, 9), 1009)

    // Identity and behavior survive GC.
    fullGC()
    assert.eq(instA.exports.t.get(0) === instB.exports.foo, true)
    assert.eq(instA.exports.t.get(0)(11), 1011)
    assert.eq(instA.exports.callViaImportedTable(1, 11), 2011)
}

await assert.asyncTest(test())
