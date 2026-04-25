import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $func (param f32 f32 f32 f32 i32 i32 i32 i32 i32 i32 i32 i32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (local.get 3)
        (local.get 12)
        (local.get 13)
        (local.get 14)
        (local.get 15)
        (local.get 16)
        (local.get 17)
        (local.get 18)
        (local.get 19)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
        (f32.add)
    )

    (func $main (export "main") (param f32 f32 f32 f32 i32 i32 i32 i32 i32 i32 i32 i32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)
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
        (local.get 16)
        (local.get 17)
        (local.get 18)
        (local.get 19)
        (return_call $func)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    const { main } = instance.exports
    assert.eq(main(1, 1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, 1, 1, 1, 1, 1, 1), 12);
}

await assert.asyncTest(test())
