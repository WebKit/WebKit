import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (result i32)
        (local i32)
        (loop
            (local.get 0)
            (local.get 1)
            (i32.add)
            (local.set 1)
            (local.get 0)
            (i32.const 1)
            (i32.sub)
            (local.tee 0)
            (i32.const 0)
            (i32.gt_s)
            (br_if 0)
        )
        (local.get 1)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(1), 1)
    assert.eq(test(2), 3)
    assert.eq(test(3), 6)
    assert.eq(test(4), 10)
    assert.eq(test(5), 15)
}

assert.asyncTest(test())
