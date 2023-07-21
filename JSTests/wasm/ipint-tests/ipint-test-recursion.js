import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// "To understand recursion, you must first understand recursion - 1" - Ecpi

let wat = `
(module
    (func $recur (export "recur") (param i32) (result i32)
        (local.get 0)
        (i32.eqz)
        (if (result i32)
            (then
                (local.get 0)
            )
            (else
                (local.get 0)
                (i32.const 1)
                (i32.sub)
                (call $recur)
                (local.get 0)
                (i32.add)
            )
        )
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { recur } = instance.exports
    assert.eq(recur(0), 0)
    assert.eq(recur(2), 3)
    assert.eq(recur(4), 10)
}

assert.asyncTest(test())
