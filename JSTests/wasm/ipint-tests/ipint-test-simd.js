import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (result i32)
        (v128.const i32x4 2 3 4 5)
        (i32x4.extract_lane 0)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(), 2)
}

await assert.asyncTest(test())
