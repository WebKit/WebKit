import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (result i32)
        (local.get 0)
        (loop (param i32) (result i32)
            (i32.const 1)
            (i32.add)
            (local.tee 0)
            (local.get 0)
            (i32.const 5)
            (i32.sub)
            (br_if 0)
        )
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(0), 5)
    assert.eq(test(3), 5)
}

assert.asyncTest(test())
