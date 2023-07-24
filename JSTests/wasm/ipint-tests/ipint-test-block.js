import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (result i32)
        (block (result i32 i32)
            (i32.const 1)
            (block (result i32)
                (i32.const 1)
                (i32.const 2)
                (i32.const 3)
                (br 1)
                (drop)
                (i32.const 2)
                (i32.add)
            )
            (i32.const 2)
            (i32.const 3)
            (br 0)
            (drop)
            (drop)
            (i32.const 2)
            (i32.add)
        )
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(0), 3)
}

assert.asyncTest(test())
