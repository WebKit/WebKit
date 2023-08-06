import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $i2i (func (param i32) (result i32)))
    (table $table 3 funcref)
    (table $table2 3 funcref)
    (elem 3 3 3)
    (elem $elems 4 4 4)
    (func (export "init") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (table.init $table $elems)
    )
    (func (export "read") (result i32)
        (i32.const 0)
        (table.get $table)
        (ref.is_null)
    )
    (func (export "call") (param i32) (result i32)
        (local.get 0)
        (i32.const 0)
        (call_indirect $table2 (type $i2i))
    )
    (func (export "copy") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (table.copy $table2 $table)
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
    const { init, read, call, copy, inc } = instance.exports
    init(0, 0, 2);
    copy(0, 0, 3);
    assert.eq(call(5), 6);
}

assert.asyncTest(test())
