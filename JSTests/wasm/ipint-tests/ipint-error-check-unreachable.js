import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test")
        (unreachable)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.throws(() => {test()}, WebAssembly.RuntimeError, "Unreachable code should not be executed (evaluating 'test()')");
}

assert.asyncTest(test())
