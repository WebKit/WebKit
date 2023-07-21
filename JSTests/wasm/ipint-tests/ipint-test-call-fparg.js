import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $inc (export "inc") (param f32) (result f32)
        (local.get 0)
        (f32.const 1)
        (f32.add)
        (return)
    )
    (func (export "test") (param f32) (result f32)
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
    assert.eq(test(2.0), 4.0)
    assert.eq(test(3.0), 5.0)
}

assert.asyncTest(test())
