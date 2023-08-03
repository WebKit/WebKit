import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param f32) (result i32)
        (local.get 0)
        (i32.trunc_f32_s)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { memory, test } = instance.exports
    test(4294967296)
}

assert.asyncTest(test())
