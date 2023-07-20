import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "add") (param f32 i32) (result f32)
        (local.get 0)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { add }= instance.exports
    assert.eq(add(1.25, 2), 1.25)
}

assert.asyncTest(test())
