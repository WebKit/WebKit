import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (func (export "size") (result i32)
        (memory.size)
    )
    (func (export "grow") (param i32) (result i32)
        (local.get 0)
        (memory.grow)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { memory, size, grow } = instance.exports
    assert.eq(size(), 1);
    assert.eq(grow(1), 1);
    assert.eq(size(), 2);
}

assert.asyncTest(test())
