import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $i2i (func (param i32) (result i32)))
    (table $table (export "table") 3 funcref)
    (elem 3 3 3)
    (elem $elems 4 4 4)
    (func (export "init") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (table.init 0 $elems)
    )
    (func (export "read") (result i32)
        (i32.const 0)
        (table.get $table)
        (ref.is_null)
    )
    (func (export "call") (param i32) (result i32)
        (local.get 0)
        (i32.const 0)
        (call_indirect $table (type $i2i))
    )
    (func $null (param i32) (result i32)
        (local.get 0)
    )
    (func $inc (export "inc") (param i32) (result i32)
        (local.get 0)
        (i32.const 1)
        (i32.add)
    )
    (func $drop (export "drop")
        (elem.drop $elems)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { init, read, drop, call, inc } = instance.exports
    assert.eq(read(), 1);
    init(0, 0, 1);
    assert.eq(read(), 0);
    assert.eq(call(5), 6);
    drop();
    assert.throws(() => {init(0, 0, 1)}, WebAssembly.RuntimeError, "Out of bounds table access (evaluating 'init(0, 0, 1)'")
}

assert.asyncTest(test())
