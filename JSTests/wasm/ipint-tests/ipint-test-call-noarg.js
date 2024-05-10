import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $inner (export "inner") (result i32)
        (i32.const 5)
        (return)
    )
    (func (export "test") (result i32)
        (call $inner)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(), 5)
}

await assert.asyncTest(test())
