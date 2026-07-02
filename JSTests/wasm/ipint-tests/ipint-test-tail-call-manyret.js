import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $helper (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        (i32.const 0)
        (i32.const 1)
        (i32.const 2)
        (i32.const 3)
        (i32.const 4)
        (i32.const 5)
        (i32.const 6)
        (i32.const 7)
        (i32.const 8)
        (i32.const 9)
        (i32.const 10)
        (i32.const 11)
    )

    (func $main (export "main") (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        (return_call $helper)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    const { main } = instance.exports
    print(main());
    assert.eq(main(), [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]);
}

await assert.asyncTest(test())
