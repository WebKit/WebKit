import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (type (;0;) (func (param i32 i64 i32 f32 f64 f32 i64 f64 f64 i64 i64 i64 f32 i32 f32 i64 i64 f64 i64 f64 i64 f32 f32 f32 i64 i32 i32 i32 f64 i64 i64 i32 i64 f32 f64 f64 f32 f32 i32 i64) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)))
  (type (;1;) (func (param i32 i64 f64 f32 f64 i64 i32 i64 i64 i64 i64 f64 i32 i32 f32 i64 i32 i64 f64 f32 f64 f32 i32 i64 f32 f32 f32 f32) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)))
  (type (;2;) (func (param i64 f32 i64 i32 f64 f64 i32 f64 i64 f32 f64 f64 f32 i32 i32 i64 f64 f32 f32 f64 f64 i32 f32 i32 f64 i32 i64 f64 f32 i64 f64 i64 f32) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)))
  (type (;3;) (func (param f32 i32 f32 i32 i64 i32 i32 i64 f32 i64 i64 f32 i32 f64 i64 f32 f64 f64 f32 f64 f64 f64 f64 f64 f32 f32 i32 f32 i64 f64 f32 f64 i64 i64 i32 i32 i32 f64) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)))
  (type (;4;) (func (param i32 i64 f32 i32 i64 i32 f64 f32 i32 i64 i32 f32 i64 f64 i64 f32 i32 f64 f32 f32 f32 f32 i64 f64 f64 i64 f64 i64 i64 f64 f64 i64 f32 i32 f64 f64 i32 i32 i32 f32 f32) (result i64 f32 f64 i32 i32 i32 f64 i64 f32 i64 f64 i32 f64 i64 f64 f32 f32 i64 f32 i32 i64 f64 i64 f32 i64 i64 i32 i64 f64 i32 f64 i64 f64 i32)))
  (type (;5;) (func (param f64 i32 f64 i64 f32 i64 i64 f64 i32 i32 f32 f64 f32 f32) (result i64 f32 f64 i32 i32 i32 f64 i64 f32 i64 f64 i32 f64 i64 f64 f32 f32 i64 f32 i32 i64 f64 i64 f32 i64 i64 i32 i64 f64 i32 f64 i64 f64 i32)))
  (type (;6;) (func (param i64 i32 f64 f32 i64 f32 i32 f64 f32 f64 f64 f32 f32 i64 i32 f64 f32 i64 f32 f64 i32 f32 i32 f64 f32 f32 i64 f64 i64 i32 f64 i64 i64 i32 i32 f32) (result i64 f32 f64 i32 i32 i32 f64 i64 f32 i64 f64 i32 f64 i64 f64 f32 f32 i64 f32 i32 i64 f64 i64 f32 i64 i64 i32 i64 f64 i32 f64 i64 f64 i32)))
  (type (;7;) (func (param f32 i32 f32 f64 i32 f64 f32 i64 i64) (result i64 f32 f64 i32 i32 i32 f64 i64 f32 i64 f64 i32 f64 i64 f64 f32 f32 i64 f32 i32 i64 f64 i64 f32 i64 i64 i32 i64 f64 i32 f64 i64 f64 i32)))
  (type (;8;) (func (param i64 i32 i32 f32 f64 f32 i32 i32 f32 f32 f32 i32 f32 i32 i32 f64 i32 i32) (result i32 f64 i32 f32 i32 i32 i64 f64 i64 i32 f32)))
  (type (;9;) (func (param i64 i32 i32 i32 i64 f64 f64 i32 i32 i64 f32 f32 i32 i64 i64 i32 f64 f32 f32 f64 f32 f32 f32 i64 f64 i32 i64 i64 i64 f32 f64 f32 f32 f64 i32 f32 i64 i64) (result i32 f64 i32 f32 i32 i32 i64 f64 i64 i32 f32)))
  (type (;10;) (func (param f32 i32 i32 i32 f32 f64 f64 i64 i64 f64 f32 f64 i32 i64 f64 f64 i32 f64 i32 f64 i64) (result i32 f64 i32 f32 i32 i32 i64 f64 i64 i32 f32)))
  (type (;11;) (func (param f32 f32 i32) (result i32 f64 i32 f32 i32 i32 i64 f64 i64 i32 f32)))
  (type (;12;) (func (param f32 f32 f64 i64 f32 f32 f64 f32 i32 f64 f64 f64 f32 i32 f32 i32) (result f32 f32 i64 f32 f32 f32 f32 f64 f32 f32 f64 f64 i32 f64 f64 i32 f32 i32 f64 f32 i64 i64 f32 i32 f32 i64 i64 i64)))
  (type (;13;) (func (param f64 f64 f64 i32 i32 f32 i64 f64 i64 i64 i64 f32 i64 i64 i32 f32 f64 f32 i32 f64 i32 f32 i32 i32 i64 i64 f32 i32 f64 i32 i64 i64 i32 f32 f32 f64 f64 f32 i32 f64) (result f32 f32 i64 f32 f32 f32 f32 f64 f32 f32 f64 f64 i32 f64 f64 i32 f32 i32 f64 f32 i64 i64 f32 i32 f32 i64 i64 i64)))
  (type (;14;) (func (param f32 f32 f32 i32 f64 i64 f64 f32 i32 f32 i64 f32 f64 f32 f64 f64) (result f32 f32 i64 f32 f32 f32 f32 f64 f32 f32 f64 f64 i32 f64 f64 i32 f32 i32 f64 f32 i64 i64 f32 i32 f32 i64 i64 i64)))
  (type (;15;) (func (param f32 i32 f32 i32 i32 i32 i64 i32 f32 f32 i32 f32 i32 i64 i32 i32 i32 i64 f64 f64 f64 i64 f64 f32 f32 i64 f32 f64 i64 f32 f64) (result f32 f32 i64 f32 f32 f32 f32 f64 f32 f32 f64 f64 i32 f64 f64 i32 f32 i32 f64 f32 i64 i64 f32 i32 f32 i64 i64 i64)))
  (type (;16;) (func (param f32 f64 i64 f64 i64 f32 i64 i64 i32 i32 f32 f64 i32 f32 i32 f32 i32 f32 i32 i64 i32 i64 i64 f32 f64 i32 f64 i32 f64 i64 i64 f64 f64 i32) (result i64 f32 i64 f64 f64 f32 i32 i64 f64 f32 i64 f64 f64 i64 i64 f64 f32 i32 i32 i64 i32 f64 i64 f32 f32 i32 f32)))
  (type (;17;) (func (param i32 f32 i64 f32 i32) (result i64 f32 i64 f64 f64 f32 i32 i64 f64 f32 i64 f64 f64 i64 i64 f64 f32 i32 i32 i64 i32 f64 i64 f32 f32 i32 f32)))
  (type (;18;) (func (param f64 i64 f64 f64 f32 i64 f64 i32 i32 i32 i32 i64 f64 f64 f64 f32 i32 f32 i32 f64 i32 f64 i64 f32 f64 f64 i32 i64 f32 i32 f64) (result i64 f32 i64 f64 f64 f32 i32 i64 f64 f32 i64 f64 f64 i64 i64 f64 f32 i32 i32 i64 i32 f64 i64 f32 f32 i32 f32)))
  (type (;19;) (func (param i64 f64 f64 i32 f64 f64 i64 f32 f32 f64 i32) (result i64 f32 i64 f64 f64 f32 i32 i64 f64 f32 i64 f64 f64 i64 i64 f64 f32 i32 i32 i64 i32 f64 i64 f32 f32 i32 f32)))
  (type (;20;) (func (param i64 i32 f64 i32 i32 i64 f32 f32 i64 f64 f32 f32 f64 f32) (result f32 i32 f64 i32 i32 i32 i64 f32 i64 i32 f64 f32 i64 i64 f64 i64 i64 f64 i32 f64 i64 i32 i64 f64 i32 f64 i64 i64 i64 f32 f64 i64 f64 f32 f32 f64 i64 i32)))
  (type (;21;) (func (param f32 f32 i32 f32 f64 f32 i64 i64 f64 i32 i32 i64 f64 f64 f64 i32 i32 i64 i32 i32 f64 f64 i64) (result f32 i32 f64 i32 i32 i32 i64 f32 i64 i32 f64 f32 i64 i64 f64 i64 i64 f64 i32 f64 i64 i32 i64 f64 i32 f64 i64 i64 i64 f32 f64 i64 f64 f32 f32 f64 i64 i32)))
  (type (;22;) (func (param f32 i32 f64 f32 f32 f64 f64 f64) (result f32 i32 f64 i32 i32 i32 i64 f32 i64 i32 f64 f32 i64 i64 f64 i64 i64 f64 i32 f64 i64 i32 i64 f64 i32 f64 i64 i64 i64 f32 f64 i64 f64 f32 f32 f64 i64 i32)))
  (type (;23;) (func (param i32 f64 i32 i32 f32) (result f32 i32 f64 i32 i32 i32 i64 f32 i64 i32 f64 f32 i64 i64 f64 i64 i64 f64 i32 f64 i64 i32 i64 f64 i32 f64 i64 i64 i64 f32 f64 i64 f64 f32 f32 f64 i64 i32)))
  (func (;0;) (type 0) (param i32 i64 i32 f32 f64 f32 i64 f64 f64 i64 i64 i64 f32 i32 f32 i64 i64 f64 i64 f64 i64 f32 f32 f32 i64 i32 i32 i32 f64 i64 i64 i32 i64 f32 f64 f64 f32 f32 i32 i64) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)
    local.get 1
    local.get 6
    local.get 9
    local.get 4
    local.get 7
    local.get 0
    local.get 2
    local.get 10
    local.get 11
    local.get 8
    local.get 15
    local.get 13
    local.get 25
    local.get 3
    local.get 26
    local.get 17
    local.get 27
    local.get 19)
  (func (;1;) (type 1) (param i32 i64 f64 f32 f64 i64 i32 i64 i64 i64 i64 f64 i32 i32 f32 i64 i32 i64 f64 f32 f64 f32 i32 i64 f32 f32 f32 f32) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)
    local.get 0
    local.get 1
    local.get 6
    local.get 3
    local.get 2
    local.get 14
    local.get 5
    local.get 4
    local.get 11
    local.get 7
    local.get 8
    local.get 9
    local.get 19
    local.get 12
    local.get 21
    local.get 10
    local.get 15
    local.get 18
    local.get 17
    local.get 20
    local.get 23
    local.get 24
    local.get 25
    local.get 26
    local.get 1
    local.get 13
    local.get 16
    local.get 22
    f64.const 0x1.cp+4 (;=28;)
    i64.const 29
    i64.const 30
    i32.const 31
    i64.const 32
    f32.const 0x1.08p+5 (;=33;)
    f64.const 0x1.1p+5 (;=34;)
    f64.const 0x1.18p+5 (;=35;)
    f32.const 0x1.2p+5 (;=36;)
    f32.const 0x1.28p+5 (;=37;)
    i32.const 38
    i64.const 39
    call 0)
  (func (;2;) (type 2) (param i64 f32 i64 i32 f64 f64 i32 f64 i64 f32 f64 f64 f32 i32 i32 i64 f64 f32 f32 f64 f64 i32 f32 i32 f64 i32 i64 f64 f32 i64 f64 i64 f32) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)
    local.get 3
    local.get 0
    local.get 4
    local.get 1
    local.get 5
    local.get 2
    local.get 6
    local.get 8
    local.get 15
    local.get 26
    local.get 29
    local.get 7
    local.get 13
    local.get 14
    local.get 9
    local.get 31
    local.get 21
    local.get 0
    local.get 10
    local.get 12
    local.get 11
    local.get 17
    local.get 23
    local.get 2
    local.get 18
    local.get 22
    local.get 28
    local.get 32
    return_call 1)
  (func (;3;) (type 3) (param f32 i32 f32 i32 i64 i32 i32 i64 f32 i64 i64 f32 i32 f64 i64 f32 f64 f64 f32 f64 f64 f64 f64 f64 f32 f32 i32 f32 i64 f64 f32 f64 i64 i64 i32 i32 i32 f64) (result i64 i64 i64 f64 f64 i32 i32 i64 i64 f64 i64 i32 i32 f32 i32 f64 i32 f64)
    local.get 4
    local.get 0
    local.get 7
    local.get 1
    local.get 13
    local.get 16
    local.get 3
    local.get 17
    local.get 9
    local.get 2
    local.get 19
    local.get 20
    local.get 8
    local.get 5
    local.get 6
    local.get 10
    local.get 21
    local.get 11
    local.get 15
    local.get 22
    local.get 23
    local.get 12
    local.get 18
    local.get 26
    local.get 29
    local.get 34
    local.get 14
    local.get 31
    local.get 24
    local.get 28
    local.get 37
    local.get 32
    local.get 25
    return_call 2)
  (table (;0;) 24 funcref)
  (export "tail_0" (func 0))
  (export "depth0->tail_0" (func 1))
  (export "depth1->depth0->tail_0" (func 2))
  (export "depth2->depth1->depth0->tail_0" (func 3))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    let params = [2,24n,22.795953519260273,28.051830291748047,23.082874148273675,6n,12,21n,21n,5n,11n,4.323373198531815,0,7,25.667137145996094,3n,12,1n,0.5369340274705046,13.407757759094238,15.969239745320632,6.407567024230957,18,19n,2.1413168907165527,10.105082511901855,26.652294158935547,30.503873825073242]
    let res = instance.exports['depth0->tail_0'](...params);
    assert.eq(res, [24n,6n,21n,22.795953519260273,23.082874148273675,2,12,21n,5n,4.323373198531815,11n,0,7,28.051830291748047,12,0.5369340274705046,18,15.969239745320632]);
}

await assert.asyncTest(test())
