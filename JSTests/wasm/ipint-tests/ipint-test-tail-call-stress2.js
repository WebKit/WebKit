import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (type (;0;) (func (param f64 i64 i64 i64 i32 i64 f64 i64 i32 i32 i64 f32 f64) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)))
  (type (;1;) (func (param i32 f64 i64 i32 i32 i32 f32 f64 i32 f64 i32 f64 f32 i64 i64 f32 i32 i32 i64 i32 f64 f32 i32 f32) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)))
  (type (;2;) (func (param i64 f64 f64 f64 f64 i32 f32 f32 f32 f64 i64 i32 i32 i32 i32 f32 f32 f64 i32 f32 f64 f64 i32 i32 f32 f32 f64 f32 f32 f64 i64) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)))
  (type (;3;) (func (param i32 f32 f32 i64 i64 f64 i64 i32 i32 i32 i64 i32 f32 i64 i32 f32 i32 i64 f64 f32 i32 i64 f32) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)))
  (type (;4;) (func (param i64 f64 i32) (result f64 i32 f32 i32 i64 f64 f32 i64 f32 i64 f32 i32 f32 i32 f64 i32 f32 f32 f32 i32 i32 i64 i32 f32 f32 f32 i32 i64 i32 f64 i32 i32 i64 f64 i64)))
  (type (;5;) (func (param i64 i32 f32 i32 i32 f32 f64) (result f64 i32 f32 i32 i64 f64 f32 i64 f32 i64 f32 i32 f32 i32 f64 i32 f32 f32 f32 i32 i32 i64 i32 f32 f32 f32 i32 i64 i32 f64 i32 i32 i64 f64 i64)))
  (type (;6;) (func (param i64 f32 f32 f32 i64) (result f64 i32 f32 i32 i64 f64 f32 i64 f32 i64 f32 i32 f32 i32 f64 i32 f32 f32 f32 i32 i32 i64 i32 f32 f32 f32 i32 i64 i32 f64 i32 i32 i64 f64 i64)))
  (type (;7;) (func (param f64 f64 i32 f64 f32 i64 f32 f64 i32 f32 f32 f32 f64 i64) (result f64 i32 f32 i32 i64 f64 f32 i64 f32 i64 f32 i32 f32 i32 f64 i32 f32 f32 f32 i32 i32 i64 i32 f32 f32 f32 i32 i64 i32 f64 i32 i32 i64 f64 i64)))
  (type (;8;) (func (param f64 i32 i32 i32 f32 f32 f64 f64 i64 i64 i32 f32 f32 i32 f32 i64 f64 f64 i32 i32 i32 i32 i64 f64 i32 f32 f64 i32 i64 i64 f64) (result f32 i64 f64 f64 i32 f32 i32 i32 i64 i64 f64 i32 f32 i64 f32 i32 f32 f32 f64 f64 f64 f64 f32 i32 f64 i32 i32 f32 f32 i64 f64 i64 i32 f32 f64 i64 f32 f64 i32 i64 f32)))
  (type (;9;) (func (param i64 f64 f32 i64 i32 i64 f64 f64 i64 f64 i32 i32 i64 i64 i32 i32 f32 f64 i64 i32 i64 i32 i64 i32 f64 i64 f32 i32 i64 i32 f32 f32 i64 f32 i64 i64 f32 f32) (result f32 i64 f64 f64 i32 f32 i32 i32 i64 i64 f64 i32 f32 i64 f32 i32 f32 f32 f64 f64 f64 f64 f32 i32 f64 i32 i32 f32 f32 i64 f64 i64 i32 f32 f64 i64 f32 f64 i32 i64 f32)))
  (type (;10;) (func (param i64 f32 i32 f64 i64 f32 f64 f64 f64 f32 i64 i32 i64 f64 f64 f32 i32 f32 i64 i64 i32 i32 i64) (result f32 i64 f64 f64 i32 f32 i32 i32 i64 i64 f64 i32 f32 i64 f32 i32 f32 f32 f64 f64 f64 f64 f32 i32 f64 i32 i32 f32 f32 i64 f64 i64 i32 f32 f64 i64 f32 f64 i32 i64 f32)))
  (type (;11;) (func (param f64 f32 i64 f64 f32 i32 f64 i64 i64 i32 f64 f32 i64 f64 i64 i64 f32 f64 i64 i64 f32 i32 f32 f64 f64 i32 i32 f64 f64 i64 f64 i64 f32 f32 f64 i32) (result f32 i64 f64 f64 i32 f32 i32 i32 i64 i64 f64 i32 f32 i64 f32 i32 f32 f32 f64 f64 f64 f64 f32 i32 f64 i32 i32 f32 f32 i64 f64 i64 i32 f32 f64 i64 f32 f64 i32 i64 f32)))
  (type (;12;) (func (param f32 f32 i32 f32 i32) (result i32 f64 i64 f64 f64 f64 f32 f32 i32 f64 f32 f64 f64 f32 i32 f32 i32 f64 f64 f32 f64 i32 i32 i32 f64 i64 i32 i32 f64 i32 i32 i32 i32)))
  (type (;13;) (func (param f64 i64 f64 i32 i64 i32 f64 f32 f64 i32 f64 f64 i32 i64 f32 f32) (result i32 f64 i64 f64 f64 f64 f32 f32 i32 f64 f32 f64 f64 f32 i32 f32 i32 f64 f64 f32 f64 i32 i32 i32 f64 i64 i32 i32 f64 i32 i32 i32 i32)))
  (type (;14;) (func (param f32 i32 f64 i32 f32 i64 f32 f64 i32 f64 i32 f32 f32 f32 i64 f64 f32 i64 f32 i32 f32 f64 i32 f64 f32 f64 i64 i32 i64 f64 i32 f64 i64 f32 i64 i64 i32) (result i32 f64 i64 f64 f64 f64 f32 f32 i32 f64 f32 f64 f64 f32 i32 f32 i32 f64 f64 f32 f64 i32 i32 i32 f64 i64 i32 i32 f64 i32 i32 i32 i32)))
  (type (;15;) (func (param f64 i64 i32 f64 i32 f32 f64 f32 f64) (result i32 f64 i64 f64 f64 f64 f32 f32 i32 f64 f32 f64 f64 f32 i32 f32 i32 f64 f64 f32 f64 i32 i32 i32 f64 i64 i32 i32 f64 i32 i32 i32 i32)))
  (type (;16;) (func (param f64 i64 i32 f64 f32 i32 i32 f32 f32 f64 i64 f64 f64 f32 f32 i32 f64 f64 i64 f32 i32 i64 i64 i32 f64 i64) (result f64 f32 i32 i64 f64 f32 i64 f64 f64 f32 i32 f64 i64 i32 i64 f64 i32 f64 f32 i32 i64 f32 f32 f64 i64 i32 f32 i32 f64 f64 i64 f64 f32 i64 i64 f64 i64 f32 i64 f32 f32)))
  (type (;17;) (func (param f64 f64 f32 i32 i32 i64 i64 f32 i32 f64 f64 f32 f32 f32 i64 i32 i32 f64 f32 i32 f32) (result f64 f32 i32 i64 f64 f32 i64 f64 f64 f32 i32 f64 i64 i32 i64 f64 i32 f64 f32 i32 i64 f32 f32 f64 i64 i32 f32 i32 f64 f64 i64 f64 f32 i64 i64 f64 i64 f32 i64 f32 f32)))
  (type (;18;) (func (param f64 f64 f32 f64 f64 i32 f64 i32 f32 i64 i32 i64 f32 f32 i64 f32 i32 i32 i32 i32 f32 i64 f64 f64 i64 i64 i64 i32 i64 f32 i32 f64 f64 i32 i64 i64 i64 f32 f32) (result f64 f32 i32 i64 f64 f32 i64 f64 f64 f32 i32 f64 i64 i32 i64 f64 i32 f64 f32 i32 i64 f32 f32 f64 i64 i32 f32 i32 f64 f64 i64 f64 f32 i64 i64 f64 i64 f32 i64 f32 f32)))
  (type (;19;) (func (param f32 i64 i32 f64 f64 f64 i32 f32 i64 f32 f32 f32 f64 f32 i32 f32 f32 f64 i32 i32 i64 f64 f32 f64 i64 i32) (result f64 f32 i32 i64 f64 f32 i64 f64 f64 f32 i32 f64 i64 i32 i64 f64 i32 f64 f32 i32 i64 f32 f32 f64 i64 i32 f32 i32 f64 f64 i64 f64 f32 i64 i64 f64 i64 f32 i64 f32 f32)))
  (type (;20;) (func (param i32 i32 f32 f32 f32 i32 f32 f64 i64 f64 f64 i64 i32 f64 i32 i64 i64 i64 i32 i64 f64 f64 f32 f64 i64 f64) (result f64 f32 f32 i64 f32 f32 f64 f32 i64 f32 f32 f32 i64 f32 i32 i64 f64 i64 f64 i64 i64 i64 f64 f64)))
  (type (;21;) (func (param f32 f64 f64) (result f64 f32 f32 i64 f32 f32 f64 f32 i64 f32 f32 f32 i64 f32 i32 i64 f64 i64 f64 i64 i64 i64 f64 f64)))
  (type (;22;) (func (param i32 i64 f32 i64 f64 f64 i64 i64 f64) (result f64 f32 f32 i64 f32 f32 f64 f32 i64 f32 f32 f32 i64 f32 i32 i64 f64 i64 f64 i64 i64 i64 f64 f64)))
  (type (;23;) (func (param f64 i32 f32 i64 f32 i64 f32 i32 f64 f32 f64 i32 i64 i64 f64 i64 i64 f64 i32 i32) (result f64 f32 f32 i64 f32 f32 f64 f32 i64 f32 f32 f32 i64 f32 i32 i64 f64 i64 f64 i64 i64 i64 f64 f64)))
  (func (;0;) (type 0) (param f64 i64 i64 i64 i32 i64 f64 i64 i32 i32 i64 f32 f64) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 1
    local.get 4
    local.get 2
    local.get 8
    local.get 3
    local.get 9
    local.get 11
    local.get 4
    local.get 0
    local.get 6
    local.get 12
    local.get 11
    local.get 8
    f32.const 0x1.ap+3 (;=13;)
    f32.const 0x1.cp+3 (;=14;)
    i32.const 15
    f64.const 0x1p+4 (;=16;)
    f64.const 0x1.1p+4 (;=17;)
    i64.const 18
    i32.const 19
    f32.const 0x1.4p+4 (;=20;)
    f64.const 0x1.5p+4 (;=21;)
    i32.const 22
    i64.const 23
    f64.const 0x1.8p+4 (;=24;)
    i32.const 25
    i64.const 26
    i32.const 27
    f32.const 0x1.cp+4 (;=28;)
    f32.const 0x1.dp+4 (;=29;)
    f32.const 0x1.ep+4 (;=30;)
    f64.const 0x1.fp+4 (;=31;)
    f64.const 0x1p+5 (;=32;)
    i32.const 33
    f32.const 0x1.1p+5 (;=34;))
  (func (;1;) (type 1) (param i32 f64 i64 i32 i32 i32 f32 f64 i32 f64 i32 f64 f32 i64 i64 f32 i32 i32 i64 i32 f64 f32 i32 f32) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 1
    local.get 2
    local.get 13
    local.get 14
    local.get 0
    local.get 18
    local.get 7
    local.get 2
    local.get 3
    local.get 4
    local.get 13
    local.get 6
    local.get 9
    return_call 0)
  (func (;2;) (type 2) (param i64 f64 f64 f64 f64 i32 f32 f32 f32 f64 i64 i32 i32 i32 i32 f32 f32 f64 i32 f32 f64 f64 i32 i32 f32 f32 f64 f32 f32 f64 i64) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 5
    local.get 1
    local.get 0
    local.get 11
    local.get 12
    local.get 13
    local.get 6
    local.get 2
    local.get 14
    local.get 3
    local.get 18
    local.get 4
    local.get 7
    local.get 10
    local.get 30
    local.get 8
    local.get 22
    local.get 23
    local.get 0
    local.get 5
    local.get 9
    local.get 15
    local.get 11
    local.get 16
    return_call 1)
  (func (;3;) (type 3) (param i32 f32 f32 i64 i64 f64 i64 i32 i32 i32 i64 i32 f32 i64 i32 f32 i32 i64 f64 f32 i32 i64 f32) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 3
    local.get 5
    local.get 18
    local.get 5
    local.get 18
    local.get 0
    local.get 1
    local.get 2
    local.get 12
    local.get 5
    local.get 4
    local.get 7
    local.get 8
    local.get 9
    local.get 11
    local.get 15
    local.get 19
    local.get 18
    local.get 14
    local.get 22
    local.get 5
    local.get 18
    local.get 16
    i32.const 23
    f32.const 0x1.8p+4 (;=24;)
    f32.const 0x1.9p+4 (;=25;)
    f64.const 0x1.ap+4 (;=26;)
    f32.const 0x1.bp+4 (;=27;)
    f32.const 0x1.cp+4 (;=28;)
    f64.const 0x1.dp+4 (;=29;)
    i64.const 30
    return_call 2)
  (func (;4;) (type 0) (param f64 i64 i64 i64 i32 i64 f64 i64 i32 i32 i64 f32 f64) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 1
    local.get 4
    local.get 2
    local.get 8
    local.get 3
    local.get 9
    local.get 11
    local.get 4
    local.get 0
    local.get 6
    local.get 12
    local.get 11
    local.get 8
    f32.const 0x1.ap+3 (;=13;)
    f32.const 0x1.cp+3 (;=14;)
    i32.const 15
    f64.const 0x1p+4 (;=16;)
    f64.const 0x1.1p+4 (;=17;)
    i64.const 18
    i32.const 19
    f32.const 0x1.4p+4 (;=20;)
    f64.const 0x1.5p+4 (;=21;)
    i32.const 22
    i64.const 23
    f64.const 0x1.8p+4 (;=24;)
    i32.const 25
    i64.const 26
    i32.const 27
    f32.const 0x1.cp+4 (;=28;)
    f32.const 0x1.dp+4 (;=29;)
    f32.const 0x1.ep+4 (;=30;)
    f64.const 0x1.fp+4 (;=31;)
    f64.const 0x1p+5 (;=32;)
    i32.const 33
    f32.const 0x1.1p+5 (;=34;))
  (func (;5;) (type 1) (param i32 f64 i64 i32 i32 i32 f32 f64 i32 f64 i32 f64 f32 i64 i64 f32 i32 i32 i64 i32 f64 f32 i32 f32) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 1
    local.get 2
    local.get 13
    local.get 14
    local.get 0
    local.get 18
    local.get 7
    local.get 2
    local.get 3
    local.get 4
    local.get 13
    local.get 6
    local.get 9
    call 4)
  (func (;6;) (type 2) (param i64 f64 f64 f64 f64 i32 f32 f32 f32 f64 i64 i32 i32 i32 i32 f32 f32 f64 i32 f32 f64 f64 i32 i32 f32 f32 f64 f32 f32 f64 i64) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 5
    local.get 1
    local.get 0
    local.get 11
    local.get 12
    local.get 13
    local.get 6
    local.get 2
    local.get 14
    local.get 3
    local.get 18
    local.get 4
    local.get 7
    local.get 10
    local.get 30
    local.get 8
    local.get 22
    local.get 23
    local.get 0
    local.get 5
    local.get 9
    local.get 15
    local.get 11
    local.get 16
    call 5)
  (func (;7;) (type 3) (param i32 f32 f32 i64 i64 f64 i64 i32 i32 i32 i64 i32 f32 i64 i32 f32 i32 i64 f64 f32 i32 i64 f32) (result i64 i32 i64 i32 i64 i32 f32 i32 f64 f64 f64 f32 i32 f32 f32 i32 f64 f64 i64 i32 f32 f64 i32 i64 f64 i32 i64 i32 f32 f32 f32 f64 f64 i32 f32)
    local.get 3
    local.get 5
    local.get 18
    local.get 5
    local.get 18
    local.get 0
    local.get 1
    local.get 2
    local.get 12
    local.get 5
    local.get 4
    local.get 7
    local.get 8
    local.get 9
    local.get 11
    local.get 15
    local.get 19
    local.get 18
    local.get 14
    local.get 22
    local.get 5
    local.get 18
    local.get 16
    i32.const 23
    f32.const 0x1.8p+4 (;=24;)
    f32.const 0x1.9p+4 (;=25;)
    f64.const 0x1.ap+4 (;=26;)
    f32.const 0x1.bp+4 (;=27;)
    f32.const 0x1.cp+4 (;=28;)
    f64.const 0x1.dp+4 (;=29;)
    i64.const 30
    call 6)

  (export "tail_0" (func 0))
  (export "depth0->tail_0" (func 1))
  (export "depth1->depth0->tail_0" (func 2))
  (export "depth2->depth1->depth0->tail_0" (func 3))
  (export "tail_4" (func 4))
  (export "depth0->tail_4" (func 5))
  (export "depth1->depth0->tail_4" (func 6))
  (export "depth2->depth1->depth0->tail_4" (func 7))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });

    let params=[27n,28.763987761288554,0.44927457229948686,1.4210862704081713,20.242422468967522,23,28.1806583404541,10.527889251708984,1.7824684381484985,4.2735349207177755,29n,0,4,5,9,4.870948314666748,5.493413925170898,12.164878104657685,24,18.075897216796875,8.106143428618541,10.693541211779017,11,2,7.056168079376221,25.168113708496094,31.425689809273926,22.665918350219727,22.22336196899414,3.6509388645220824,31n];

    let res = instance.exports['depth1->depth0->tail_4'](...params);
    assert.eq(res, [27n,23,29n,0,31n,4,28.1806583404541,23,28.763987761288554,0.44927457229948686,1.4210862704081713,28.1806583404541,0,13,14,15,16,17,18n,19,20,21,22,23n,24,25,26n,27,28,29,30,31,32,33,34]);
}

await assert.asyncTest(test())
