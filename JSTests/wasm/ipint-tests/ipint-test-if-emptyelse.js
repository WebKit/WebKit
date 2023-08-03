import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32 i32 i32) (result i32)
        (local.get 0)
        (if
            (then
                (local.get 1)
                (return)
            )
            (else
            )
        )
        (local.get 2)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(1, 1, 2), 1)
    assert.eq(test(0, 1, 2), 2)
}

assert.asyncTest(test())
