import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (type (;0;) (func (param f32 f64 i64 i32 f64 i64 f32 f64 i32 f32 i32 f64 i32 f32 f64 i32 f64 i32 f32 i64 f64 i32 i32 f64 f32 f32 i64 f32 i32 f32 i32 f64 f32 i32 i32 i64 i64 i64 i64) (result f64 i32 f32)))
  (type (;1;) (func (param) (result f64 i32 f32)))
  (func (;0;) (type 0) (param f32 f64 i64 i32 f64 i64 f32 f64 i32 f32 i32 f64 i32 f32 f64 i32 f64 i32 f32 i64 f64 i32 i32 f64 f32 f32 i64 f32 i32 f32 i32 f64 f32 i32 i32 i64 i64 i64 i64) (result f64 i32 f32)
    local.get 1
    local.get 3
    local.get 0
  )
  (func (;1;) (type 1) (result f64 i32 f32)
    f32.const 8
    f64.const 9
    i64.const 10
    i32.const 11
    f64.const 12
    i64.const 13
    f32.const 14
    f64.const 15
    i32.const 16
    f32.const 17
    i32.const 18
    f64.const 19
    i32.const 20
    f32.const 21
    f64.const 22
    i32.const 23
    f64.const 24
    i32.const 25
    f32.const 26
    i64.const 27
    f64.const 28
    i32.const 29
    i32.const 30
    f64.const 31
    f32.const 32
    f32.const 33
    i64.const 34
    f32.const 35
    i32.const 36
    f32.const 37
    i32.const 38
    f64.const 39
    f32.const 40
    i32.const 41
    i32.const 42
    i64.const 43
    i64.const 44
    i64.const 45
    i64.const 46
    return_call 0)
  (export "tail_0" (func 0))
  (export "depth0->tail_0" (func 1))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    let res = instance.exports['depth0->tail_0']();
    assert.eq(res, [9, 11, 8]);
}

await assert.asyncTest(test())
