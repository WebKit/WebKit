import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $add (export "add") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        (local.get 0)
        (local.get 1)
        (i32.add)
        (local.get 2)
        (i32.add)
        (local.get 3)
        (i32.add)
        (local.get 4)
        (i32.add)
        (local.get 5)
        (i32.add)
        (local.get 6)
        (i32.add)
        (local.get 7)
        (i32.add)
        (local.get 8)
        (i32.add)
        (local.get 9)
        (i32.add)
        (local.get 10)
        (i32.add)
        (local.get 11)
        (i32.add)
        (return)
    )
    (func (export "test") (param i32) (result i32)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (local.get 0)
        (call $add)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test, add } = instance.exports
    assert.eq(add(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1), 12);
    assert.eq(test(1), 12)
}

assert.asyncTest(test())
