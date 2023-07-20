import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "add") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.add)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { add }= instance.exports
    assert.eq(add(1, 2), 3)
    assert.eq(add(3.5, 4.25), 7.75)
}

assert.asyncTest(test())
