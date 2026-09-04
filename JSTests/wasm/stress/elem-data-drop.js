import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const elemWat = `
(module
  (table $t 1 funcref)
  (elem $e funcref (ref.func $f))
  (func $f)
  (func (export "drop")
    (elem.drop $e))
  (func (export "init")
    (table.init $t $e (i32.const 0) (i32.const 0) (i32.const 1)))
)
`

const dataWat = `
(module
  (memory 1)
  (data $d "x")
  (func (export "drop")
    (data.drop $d))
  (func (export "init")
    (memory.init $d (i32.const 0) (i32.const 0) (i32.const 1)))
)
`

async function test() {
    const elem = await instantiate(elemWat, {})
    elem.exports.init()
    elem.exports.drop()
    elem.exports.drop()
    assert.throws(() => { elem.exports.init() }, WebAssembly.RuntimeError, "Out of bounds table access")

    const data = await instantiate(dataWat, {})
    data.exports.init()
    data.exports.drop()
    data.exports.drop()
    assert.throws(() => { data.exports.init() }, WebAssembly.RuntimeError, "Out of bounds memory access")
}

await assert.asyncTest(test())
