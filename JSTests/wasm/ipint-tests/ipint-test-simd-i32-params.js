import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (param i32) (result i32)
        (v128.const i32x4 2 3 4 5)
        (i32x4.extract_lane 2)
        (local.get 0)
        (local.get 1)
        (i32.div_s)
        (i32.add)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(0, 2), 4)
}

await assert.asyncTest(test())
