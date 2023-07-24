import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "eq") (param f64 f64) (result i32)
        (local.get 0)
        (local.get 1)
        (f64.eq)
        (return)
    )
    (func (export "ne") (param f64 f64) (result i32)
        (local.get 0)
        (local.get 1)
        (f64.ne)
        (return)
    )
    (func (export "lt") (param f64 f64) (result i32)
        (local.get 0)
        (local.get 1)
        (f64.lt)
        (return)
    )
    (func (export "gt") (param f64 f64) (result i32)
        (local.get 0)
        (local.get 1)
        (f64.gt)
        (return)
    )
    (func (export "le") (param f64 f64) (result i32)
        (local.get 0)
        (local.get 1)
        (f64.le)
        (return)
    )
    (func (export "ge") (param f64 f64) (result i32)
        (local.get 0)
        (local.get 1)
        (f64.ge)
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
