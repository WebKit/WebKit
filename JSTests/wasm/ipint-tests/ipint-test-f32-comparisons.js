import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "eq") (param f32 f32) (result i32)
        (local.get 0)
        (local.get 1)
        (f32.eq)
        (return)
    )
    (func (export "ne") (param f32 f32) (result i32)
        (local.get 0)
        (local.get 1)
        (f32.ne)
        (return)
    )
    (func (export "lt") (param f32 f32) (result i32)
        (local.get 0)
        (local.get 1)
        (f32.lt)
        (return)
    )
    (func (export "gt") (param f32 f32) (result i32)
        (local.get 0)
        (local.get 1)
        (f32.gt)
        (return)
    )
    (func (export "le") (param f32 f32) (result i32)
        (local.get 0)
        (local.get 1)
        (f32.le)
        (return)
    )
    (func (export "ge") (param f32 f32) (result i32)
        (local.get 0)
        (local.get 1)
        (f32.ge)
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {});
    const { eq, ne, lt, gt, le, ge } = instance.exports;
    assert.eq(eq(1.05, 1.05), 1)
    assert.eq(eq(1.05, 1.06), 0)
    assert.eq(ne(1.05, 1.05), 0)
    assert.eq(ne(1.05, 1.06), 1)
    assert.eq(lt(1.05, 2.05), 1)
    assert.eq(lt(1.05, 0.05), 0)
    assert.eq(gt(1.05, 0.05), 1)
    assert.eq(gt(1.05, 1.05), 0)
    assert.eq(le(1.05, 1.05), 1)
    assert.eq(le(1.05, 0.05), 0)
    assert.eq(ge(1.05, 1.05), 1)
    assert.eq(ge(1.05, 2.05), 0)
}

assert.asyncTest(test())
