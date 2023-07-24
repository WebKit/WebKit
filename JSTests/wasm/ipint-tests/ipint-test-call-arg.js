import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $inc (export "inc") (param i32) (result i32)
        (local.get 0)
        (i32.const 1)
        (i32.add)
        (return)
    )
    (func (export "test") (param i32) (result i32)
        (i32.const 1094795585)
        (local.get 0)
        (call $inc)
        (call $inc)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(2), 4)
    assert.eq(test(3), 5)
}

assert.asyncTest(test())
