import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $inner (export "inner")
        (return)
    )
    (func (export "test") (result i32)
        (call $inner)
        (i32.const 5)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(), 5)
}

assert.asyncTest(test())
