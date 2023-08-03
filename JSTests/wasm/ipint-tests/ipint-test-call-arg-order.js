import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $r2 (param i32 i32) (result i32)
        (local.get 0)
        (return)
    )
    (func (export "test") (result i32)
        (i32.const 1)
        (i32.const 2)
        (call $r2)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(), 1)
}

assert.asyncTest(test())
