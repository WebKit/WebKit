import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Funcref tables initialized by an element segment are usable from both wasm
// (call_indirect) and JS (table.get), and the wrappers returned by table.get
// keep stable identity across repeated reads and across GC.

let wat = `
(module
    (type $sig (func (param i32) (result i32)))
    (table (export "t") 4 4 funcref)
    (elem (i32.const 0) $a $b $c $d)

    (func $a (param i32) (result i32) (i32.add (local.get 0) (i32.const 10)))
    (func $b (param i32) (result i32) (i32.add (local.get 0) (i32.const 20)))
    (func $c (param i32) (result i32) (i32.add (local.get 0) (i32.const 30)))
    (func $d (param i32) (result i32) (i32.add (local.get 0) (i32.const 40)))

    (func (export "callIndirect") (param i32 i32) (result i32)
        (call_indirect (type $sig) (local.get 1) (local.get 0))
    )
)
`

async function test() {
    const { t, callIndirect } = (await instantiate(wat, {}, {})).exports

    // call_indirect on slots that have never been observed from JS.
    assert.eq(callIndirect(0, 5), 15)
    assert.eq(callIndirect(1, 5), 25)
    assert.eq(callIndirect(2, 5), 35)
    assert.eq(callIndirect(3, 5), 45)

    // First JS observation: the slot becomes a callable function.
    const f0a = t.get(0)
    assert.eq(typeof f0a, "function")
    assert.eq(f0a(7), 17)

    // Repeated reads of the same slot return the same function object.
    const f0b = t.get(0)
    assert.eq(f0a === f0b, true)

    // Different slots are distinct functions.
    const f1 = t.get(1)
    assert.eq(f1 !== f0a, true)
    assert.eq(f1(7), 27)

    // Identity survives GC.
    fullGC()
    assert.eq(t.get(0) === f0a, true)
    assert.eq(t.get(1) === f1, true)

    // call_indirect still works after JS has observed (and after GC).
    assert.eq(callIndirect(0, 100), 110)
    assert.eq(callIndirect(2, 100), 130)

    // Storing a funcref via JS makes table.get return the very object set.
    t.set(2, f0a)
    assert.eq(t.get(2) === f0a, true)
    assert.eq(callIndirect(2, 5), 15)
}

await assert.asyncTest(test())
