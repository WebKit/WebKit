import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let producerWat = `
(module
    (func $hidden (export "hidden") (result i32) (i32.const 7))
)
`

let wat = `
(module
    (import "m" "hidden" (func $imported (result i32)))
    (func $local (result i32) (i32.const 11))
    (func $exported (export "exported") (result i32) (i32.const 13))
    (elem declare funcref (ref.func $imported) (ref.func $local) (ref.func $exported))
    (func (export "importedRef") (result funcref) (ref.func $imported))
    (func (export "localRef") (result funcref) (ref.func $local))
    (func (export "exportedRef") (result funcref) (ref.func $exported))
)
`

async function test() {
    const { hidden } = (await instantiate(producerWat)).exports
    const { importedRef, localRef, exportedRef, exported } = (await instantiate(wat, { m: { hidden } })).exports

    let imported = importedRef()
    let local = localRef()
    let exportedFromRef = exportedRef()

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(importedRef(), imported)
        assert.eq(localRef(), local)
        assert.eq(exportedRef(), exportedFromRef)
    }

    assert.eq(imported === local, false)
    assert.eq(local === exportedFromRef, false)
    assert.eq(imported, hidden)
    assert.eq(exportedFromRef, exported)
    assert.eq(imported(), 7)
    assert.eq(local(), 11)
    assert.eq(exportedFromRef(), 13)

    fullGC()
    assert.eq(importedRef(), imported)
    assert.eq(localRef(), local)
    assert.eq(exportedRef(), exported)
}

await assert.asyncTest(test())
