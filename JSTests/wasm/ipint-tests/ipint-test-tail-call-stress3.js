import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (type (;0;) (func (param f32 i64 i32 i32 f64 f64 i32) (result i64 i64 f64 i32 i32 f64 i64 f64 f64 i32 i64 i32 f64 f32 f64 i64)))
  (type (;1;) (func (param i32 i64 f32 i64 f32 f64 f32 f64 i32 i32 i32 f64 f64 i64 f64 i32 i64 i64 f32 f64 f32 i32 i64 i64 f64 f64) (result i64 i64 f64 i32 i32 f64 i64 f64 f64 i32 i64 i32 f64 f32 f64 i64)))
  (type (;2;) (func (param f64 f32 f32 i32 f64) (result i64 i64 f64 i32 i32 f64 i64 f64 f64 i32 i64 i32 f64 f32 f64 i64)))
  (type (;3;) (func (param i32 i32 i32 f32 f64 f64 f64 i32 f64 f32 i32 i64 i32 i64 f32 i64 f64 f64 f32 f64 f32 i64 i64 f64) (result i64 i64 f64 i32 i32 f64 i64 f64 f64 i32 i64 i32 f64 f32 f64 i64)))
  (func (;0;) (type 0) (param f32 i64 i32 i32 f64 f64 i32) (result i64 i64 f64 i32 i32 f64 i64 f64 f64 i32 i64 i32 f64 f32 f64 i64)
    local.get 1
    local.get 1
    local.get 4
    local.get 2
    local.get 3
    local.get 5
    local.get 1
    f64.const 0x1.cp+2 (;=7;)
    f64.const 0x1p+3 (;=8;)
    i32.const 9
    i64.const 10
    i32.const 11
    f64.const 0x1.8p+3 (;=12;)
    f32.const 0x1.ap+3 (;=13;)
    f64.const 0x1.cp+3 (;=14;)
    i64.const 15)
  (func (;1;) (type 1) (param i32 i64 f32 i64 f32 f64 f32 f64 i32 i32 i32 f64 f64 i64 f64 i32 i64 i64 f32 f64 f32 i32 i64 i64 f64 f64) (result i64 i64 f64 i32 i32 f64 i64 f64 f64 i32 i64 i32 f64 f32 f64 i64)
    local.get 2
    local.get 1
    local.get 0
    local.get 8
    local.get 5
    local.get 7
    local.get 9
    return_call 0)
  (export "tail_0" (func 0))
  (export "depth0->tail_0" (func 1))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });

    let params = [13,25n,7.413572788238525,28n,6.910492420196533,5.8284704775604546,2.8514695167541504,6.637032240024904,29,24,10,17.346053055222885,27.06108496735929,0n,31.091655802928926,20,20n,3n,26.126325607299805,6.834118985396373,27.44361686706543,7,12n,24n,28.190403281315504,10.128554176981712];

    let result = instance.exports['depth0->tail_0'](...params);
    // expect 25,25,5.8284704775604546,13,29,6.637032240024904,25,7,8,9,10,11,12,13,14,15
    assert.eq(result, [25n,25n,5.8284704775604546,13,29,6.637032240024904,25n,7,8,9,10n,11,12,13,14,15n]);
}

await assert.asyncTest(test())
