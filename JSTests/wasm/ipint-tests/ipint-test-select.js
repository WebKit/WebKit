import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (result i32)
        (i32.const 2)
        (i32.const 1)
        (local.get 0)
        (select)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(0), 1);
    assert.eq(test(1), 2);
    assert.eq(test(2), 2);
}

assert.asyncTest(test())
