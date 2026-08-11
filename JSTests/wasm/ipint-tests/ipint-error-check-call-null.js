import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $i2i (func (param i32) (result i32)))
    (table $table (export "table") 3 funcref)
    (func (export "call") (param i32) (result i32)
        (local.get 0)
        (i32.const 0)
        (call_indirect $table (type $i2i))
    )
    (func $inc (export "inc") (param i32) (result i32)
        (local.get 0)
        (i32.const 1)
        (i32.add)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { table, read, write_null, write_inc, call, inc } = instance.exports
    assert.throws(() => {call(5)}, WebAssembly.RuntimeError, "call_indirect to a signature that does not match (evaluating 'call(5)')");
}

await assert.asyncTest(test())
