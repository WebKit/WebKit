import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $r9 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        (local.get 8)
        (return)
    )
    (func (export "test") (result i32)
        (i32.const 1)
        (i32.const 2)
        (i32.const 3)
        (i32.const 4)
        (i32.const 5)
        (i32.const 6)
        (i32.const 7)
        (i32.const 8)
        (i32.const 9)
        (i32.const 10)
        (call $r9)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(), 9)
}

assert.asyncTest(test())
