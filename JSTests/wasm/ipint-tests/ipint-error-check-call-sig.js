import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $p2i (func (param i32) (result i32 i32)))
    (table $table (export "table") 3 funcref)
    (func (export "write_inc")
        (i32.const 0)
        (ref.func $inc)
        (table.set $table)
    )
    (func (export "call") (param i32) (result i32)
        (local.get 0)
        (i32.const 0)
        (call_indirect $table (type $p2i))
        (drop)
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
    write_inc();
    assert.throws(() => {call(5)}, WebAssembly.RuntimeError, "call_indirect to a signature that does not match (evaluating 'call(5)')");
}

assert.asyncTest(test())
