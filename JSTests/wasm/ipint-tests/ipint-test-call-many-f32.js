import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test_f32_10") (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)
        (local.get 8)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test_f32_10 } = instance.exports
    assert.eq(test_f32_10(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), 9)
}

await assert.asyncTest(test())
