import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $i2i (func (param i32) (result i32)))
    (table $table (export "table") 3 funcref)
    (func (export "read") (result i32)
        (i32.const 0)
        (table.get $table)
        (ref.is_null)
    )
    (func (export "write_null")
        (i32.const 0)
        (ref.null func)
        (table.set $table)
    )
    (func (export "write_inc")
        (i32.const 0)
        (ref.func $inc)
        (table.set $table)
    )
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
    assert.eq(read(), 1);
    table.set(0, inc);
    assert.eq(read(), 0);
    write_null();
    assert.eq(read(), 1);
    write_inc();
    assert.eq(read(), 0);
    assert.eq(call(5), 6);
}

assert.asyncTest(test())
