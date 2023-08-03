import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (param i32) (result i32)
        (block
            (block
                (block
                    (block
                        (local.get 0)
                        (br_table 0 1 2 3)
                    )
                    (i32.const 2)
                    (return)
                )
                (i32.const 3)
                (return)
            )
            (i32.const 5)
            (return)
        )
        (i32.const 7)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports
    assert.eq(test(0), 2);
    assert.eq(test(1), 3);
    assert.eq(test(2), 5);
    assert.eq(test(3), 7);
    assert.eq(test(4), 7);
}

assert.asyncTest(test())
