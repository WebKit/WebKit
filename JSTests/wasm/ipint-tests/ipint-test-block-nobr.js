import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (result i32)
        (block (result i32 i32 i32 i32)
            (block (result i32 i32)
                (i32.const 1)
                (i32.const 2)
            )
            (i32.const 2)
            (i32.const 3)
        )
        (i32.add)
        (i32.add)
        (i32.add)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(), 8)
}

assert.asyncTest(test())
