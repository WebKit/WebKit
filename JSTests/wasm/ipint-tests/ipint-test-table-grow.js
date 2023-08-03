import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $i2i (func (param i32) (result i32)))
    (table $table 3 funcref)
    (elem (i32.const 0) 0 0 0)
    (func (export "size") (result i32)
        (table.size $table)
    )
    (func (export "grow") (param i32) (result i32)
        (ref.null func)
        (local.get 0)
        (table.grow $table)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { size, grow } = instance.exports;
    assert.eq(size(), 3);
    assert.eq(grow(4), 3);
    assert.eq(size(), 7);
}

assert.asyncTest(test())
