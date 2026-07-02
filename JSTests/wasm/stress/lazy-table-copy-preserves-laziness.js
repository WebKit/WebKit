import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// table.copy moves entries between slots without forcing the JS-side
// representation of a slot to be observed. After copying, the destination
// slots behave identically to the source slots: call_indirect works on them,
// table.get returns the same function object as the source, and ref.func to
// the same function also matches.

let wat = `
(module
    (type $sig (func (param i32) (result i32)))
    (table $t (export "t") 16 16 funcref)
    (elem (i32.const 0) $a $b $c $d)

    (func $a (param i32) (result i32) (i32.add (local.get 0) (i32.const 1)))
    (func $b (param i32) (result i32) (i32.add (local.get 0) (i32.const 2)))
    (func $c (param i32) (result i32) (i32.add (local.get 0) (i32.const 3)))
    (func $d (param i32) (result i32) (i32.add (local.get 0) (i32.const 4)))

    (func (export "doCopy")
        (table.copy (i32.const 8) (i32.const 0) (i32.const 4)))

    (func (export "callIndirect") (param i32 i32) (result i32)
        (call_indirect (type $sig) (local.get 1) (local.get 0)))

    (func (export "refSrc") (result funcref) (ref.func $a))
)
`

async function test() {
    const { t, doCopy, callIndirect, refSrc } = (await instantiate(wat, {}, {})).exports

    // Copy slots 0..3 to 8..11. Nothing has been observed from JS yet.
    doCopy()

    // call_indirect works on the copied slots.
    assert.eq(callIndirect(8, 100), 101)  // $a
    assert.eq(callIndirect(9, 100), 102)  // $b
    assert.eq(callIndirect(10, 100), 103) // $c
    assert.eq(callIndirect(11, 100), 104) // $d

    // Observing the destination first still produces a callable function.
    const aFromDst = t.get(8)
    assert.eq(typeof aFromDst, "function")
    assert.eq(aFromDst(50), 51)

    // Source and destination of the copy refer to the same function and
    // therefore return the same function object.
    const aFromSrc = t.get(0)
    assert.eq(aFromSrc === aFromDst, true)
    assert.eq(refSrc() === aFromDst, true)

    // A different function in the copied range gives a different object,
    // and source/destination identity holds for it too.
    const bFromDst = t.get(9)
    assert.eq(bFromDst !== aFromDst, true)
    assert.eq(t.get(1) === bFromDst, true)
    assert.eq(bFromDst(50), 52)

    fullGC()
    assert.eq(t.get(8) === aFromDst, true)
    assert.eq(callIndirect(11, 0), 4)
    assert.eq(t.get(11)(0), 4)
}

await assert.asyncTest(test())
