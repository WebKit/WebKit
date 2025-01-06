import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (local.get 3)
        (local.get 4)
        (local.get 5)
        (local.get 6)
        (local.get 7)
        (local.get 8)
        (local.get 9)
        (local.get 10)
        (local.get 11)
        (local.get 12)
        (local.get 13)
        (local.get 14)
        (local.get 15)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
        (i32.add)
    )

    (func $main (export "main") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (local.get 3)
        (local.get 4)
        (local.get 5)
        (local.get 6)
        (local.get 7)
        (local.get 8)
        (local.get 9)
        (local.get 10)
        (local.get 11)
        (local.get 8)
        (local.get 9)
        (local.get 10)
        (local.get 11)
        (return_call $func)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    const { main } = instance.exports
    assert.eq(main(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1), 16);
    assert.eq(main(1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2), 24);
}

await assert.asyncTest(test())
