import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "addi32") (param i32 i32) (result i32)
        local.get 0
        local.get 1
        i32.add)
    (func (export "identf64") (param f64) (result f64)
        local.get 0)
    (func (export "identextern") (param externref) (result externref)
        local.get 0)
    (func (export "identi64") (param i64) (result i64)
        local.get 0)
    (func (export "nop"))
)
`

async function test() {
    const { addi32, identf64, identextern, identi64, nop } = (await instantiate(wat)).exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(addi32(1, 2), 3)
        assert.eq(addi32(1), 1)
        assert.eq(addi32(), 0)
        assert.eq(addi32(1, 2, 99), 3)
        assert.eq(Object.is(identf64(), NaN), true)
        assert.eq(identf64(1.5), 1.5)
        assert.eq(identextern(), undefined)
        nop(1)
        assert.throws(() => identi64(), TypeError, "")
    }
}

await assert.asyncTest(test())
