import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $i2i (func (param i32) (result i32)))
    (table $table 5 funcref)
    (elem (i32.const 0) 0 0 0 0 0)
    (func (export "size") (result i32)
        (table.size $table)
    )
    (func (export "fill") (param i32 i32)
        (local.get 0)
        (ref.null func)
        (local.get 1)
        (table.fill $table)
    )
    (func (export "isnull") (param i32) (result i32)
        (local.get 0)
        (table.get $table)
        (ref.is_null)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { size, fill, isnull } = instance.exports;
    assert.eq(size(), 5);
    assert.eq(isnull(0), 0);
    fill(0, 5);
    assert.eq(isnull(0), 1);
}

assert.asyncTest(test())
