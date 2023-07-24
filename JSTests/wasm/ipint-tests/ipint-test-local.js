import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (result i32)
        (local $var i32)
        (local.get 0)
        (local.get 0)
        (local.get 1)
        (i32.add)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(3), 3)
    assert.eq(test(5), 5)
}

assert.asyncTest(test())
