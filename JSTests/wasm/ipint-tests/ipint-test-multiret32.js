import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test")
        (result i32) (result i32) (result i32) (result i32)
        (result i32) (result i32) (result i32) (result i32)
        (result i32) (result i32) (result i32) (result i32)
        (result i32) (result i32) (result i32) (result i32)
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
        (i32.const 12)
        (i32.const 13)
        (i32.const 14)
        (i32.const 15)
        (i32.const 16)
        (return)
    )
    (func (export "test_fp")
        (result f32) (result f32) (result f32) (result f32)
        (result f32) (result f32) (result f32) (result f32)
        (result f32) (result f32) (result f32) (result f32)
        (result f32) (result f32) (result f32) (result f32)
        (f32.const 1)
        (f32.const 2)
        (f32.const 3)
        (f32.const 4)
        (f32.const 5)
        (f32.const 6)
        (f32.const 7)
        (f32.const 8)
        (f32.const 9)
        (f32.const 10)
        (f32.const 11)
        (f32.const 12)
        (f32.const 13)
        (f32.const 14)
        (f32.const 15)
        (f32.const 16)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { test, test_fp } = instance.exports
    assert.eq(test(), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
    assert.eq(test_fp(), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
}

await assert.asyncTest(test())
