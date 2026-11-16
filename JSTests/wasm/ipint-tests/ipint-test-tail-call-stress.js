import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (type (;0;) (func (param f32 f64 i64 i32 i64 f64 f32 f32) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)))
  (type (;1;) (func (param f64 f64 f32 f64 i32 f64 f32 i64 f32 i64 f32 f32 i32 i32 f32) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)))
  (type (;2;) (func (param f32 i64 f32 f64 i32) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)))
  (type (;3;) (func (param i32 f32 f64 f32 f32 f32 f32 i64 i64 i64 i32 i32 i32 f32 i64 f64 i32 i64 i32 i32 i64 i32 f64 i64 i32 i64 f64 i64 f32 i32 f32 i32 f32 f32 f32 i64 f64) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)))
  (type (;4;) (func (param f64 f32 f32 i32 i64 i32 f32 f64 f64 i64 f32 i64 f32 i64 i64 i32 f64 f32 f64 f64 i64 f64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)))
  (type (;5;) (func (param f32 i64 f64 f32 i64 f64 f64 f64 f32 f32 f64 i64 i64 i64 i64 f64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)))
  (type (;6;) (func (param i64 f32 i64 i64 f64 i64 f64 f64 f32 f64 f64 f32 f64 f64 i64 f64 f64 i32 f64 f64 f64 f32 f32 i64 i64 f32 f32 i32 i64 f64 i32 f64 i32 i32 i64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)))
  (type (;7;) (func (param f64 i32 i32 i64 f32 i64 i64 i64 f32 i32 i64 f32 f64 f64 i64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)))
  (type (;8;) (func (param f32 i64 i64 f64 i32 f64 i64 i64 f32 f32 f32 f32 i32 i32 f32 i64 i32 f32 i64 f32 i64 i32 f32 f64 f32 f64 i64) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)))
  (type (;9;) (func (param i32 f64 f32 i64 i32 i32 i32 i64 f32 i32 f64 i64 i32 f64 f64 i64 f64 i32 i32 i32 i64 f64 f32 f32 i64 f64 i32 i32 f32 f32) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)))
  (type (;10;) (func (param f64 f32 f64 f64 i64 i32 f32 i32 f64 f64 i64 i64 i64 i64 i64 i64 i32 i64 i64 i32 f64 i64 f64 i64 i32 i64 i32 i32 f32 f64) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)))
  (type (;11;) (func (param i64 i64 f32 f32 f32 i64 i32 i64 f64 i32 i32) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)))
  (type (;12;) (func (param i32 f64 f64 f64 f32 i32 f32 f64 i32 i32 f32 i32 f64 i64 f32 i64 i32 i32 i32 f32 f32 f64 i32 i32 i64 i32 i32 f64 f32 i64 f64 f32) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)))
  (type (;13;) (func (param i32 f64 i64 f64 f64 f32 f32 f32 i64 i32 i64 f32 i64 f32 f32 i32 f64 i32 i64 f32 f64 f32 i32 i32 f32 i64 f32) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)))
  (type (;14;) (func (param f64 i64 f64 i64 f64 f64 i64 i32 f32 f32 i64 i32 f32 i32 i32 i32 f64 i32 i64 i32 i32 f64 f32 f64 i64 i64 f64 i32) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)))
  (type (;15;) (func (param i32 f32 f32 i32 f64 i32 i64 f64 i32 i32 i32 i64) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)))
  (type (;16;) (func (param i64 i64 i64 i64 f32 f32 f64 f32 i32 i32 f32 i64 i64 f64 i32 i64 i64 f64) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)))
  (type (;17;) (func (param f32 f64 f32 f64 f64 i32 f32 f64 i32 f32 i64 f64 i64 i64 i32 i64 i32 f64 i64 i32 i32 i32 f64 f32 f64 i64 i32 f32) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)))
  (type (;18;) (func (param i32 f32 i64 i32 i64 f32 f64 f64 f64 i32 i64 f64 i32 i64 i64 i32 i32 f64 i64 i32 f64 i32 i64 i64 f32 f64 i64 i32 f64 f32 f32 i64 f64 f64 f32 i32 f64) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)))
  (type (;19;) (func (param f64 i32 f32 f32 i32 i64 i32 i32 i32 f64 i64 i32 i64 f64 f64 i32 f64 i64 f64 f64) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)))
  (type (;20;) (func (param i32 f32 f32 f32 f64 i64 f64 f32 i64 i64 i32 f64 i64 i32 f64 f32 f32 i32 i64 f32 i64 i64 f32 f32 f64 i64 f64 f64 i32 f64 f64 f32 f64 i32 f64 f32 i32 i64 i32 f64) (result f32 f32 i32 i64 f32)))
  (type (;21;) (func (param f32 i32 i64 f32 i32 i64 i64 i64 i32 i32 i32 i64 f64 f32 i32 f64 f32 f32 f32 f64 f64 i32 f64 i32 i64 i64 f64 i64 i64 f64 i64 i64 f64 i64) (result f32 f32 i32 i64 f32)))
  (type (;22;) (func (param f32 f32 i32 i32 f32 i32 f64 i64 f32 i32 i64 i64 i64 i32 f32 f32 i32 f32 f64 f32 i32 i32 f32 i64 i32 f64 f32 i64 i32 i32 i32 i64) (result f32 f32 i32 i64 f32)))
  (type (;23;) (func (param f32 f64 f64 f64 f32 f64 f32 i32 i32 i64 f64 i32 f32 f64 i64 f32 i64) (result f32 f32 i32 i64 f32)))
  (func (;0;) (type 0) (param f32 f64 i64 i32 i64 f64 f32 f32) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)
    local.get 2
    local.get 3
    local.get 4
    local.get 0
    local.get 2
    local.get 1
    local.get 5
    local.get 4
    f32.const 0x1p+3 (;=8;)
    f32.const 0x1.2p+3 (;=9;)
    f64.const 0x1.4p+3 (;=10;)
    f64.const 0x1.6p+3 (;=11;)
    f32.const 0x1.8p+3 (;=12;)
    i32.const 13
    i64.const 14
    f32.const 0x1.ep+3 (;=15;)
    f64.const 0x1p+4 (;=16;)
    i64.const 17
    f64.const 0x1.2p+4 (;=18;)
    f32.const 0x1.3p+4 (;=19;)
    f32.const 0x1.4p+4 (;=20;)
    i32.const 21
    f32.const 0x1.6p+4 (;=22;)
    i32.const 23
    f64.const 0x1.8p+4 (;=24;)
    f64.const 0x1.9p+4 (;=25;)
    i32.const 26
    i64.const 27
    f32.const 0x1.cp+4 (;=28;)
    i64.const 29
    f32.const 0x1.ep+4 (;=30;)
    f64.const 0x1.fp+4 (;=31;)
    f64.const 0x1p+5 (;=32;))
  (func (;1;) (type 1) (param f64 f64 f32 f64 i32 f64 f32 i64 f32 i64 f32 f32 i32 i32 f32) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)
    local.get 2
    local.get 0
    local.get 7
    local.get 4
    local.get 9
    local.get 1
    local.get 6
    local.get 8
    return_call 0)
  (func (;2;) (type 2) (param f32 i64 f32 f64 i32) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)
    local.get 3
    local.get 3
    local.get 0
    local.get 3
    local.get 4
    f64.const 0x1.4p+2 (;=5;)
    f32.const 0x1.8p+2 (;=6;)
    i64.const 7
    f32.const 0x1p+3 (;=8;)
    i64.const 9
    f32.const 0x1.4p+3 (;=10;)
    f32.const 0x1.6p+3 (;=11;)
    i32.const 12
    i32.const 13
    f32.const 0x1.cp+3 (;=14;)
    return_call 1)
  (func (;3;) (type 3) (param i32 f32 f64 f32 f32 f32 f32 i64 i64 i64 i32 i32 i32 f32 i64 f64 i32 i64 i32 i32 i64 i32 f64 i64 i32 i64 f64 i64 f32 i32 f32 i32 f32 f32 f32 i64 f64) (result i64 i32 i64 f32 i64 f64 f64 i64 f32 f32 f64 f64 f32 i32 i64 f32 f64 i64 f64 f32 f32 i32 f32 i32 f64 f64 i32 i64 f32 i64 f32 f64 f64)
    local.get 1
    local.get 7
    local.get 3
    local.get 2
    local.get 0
    return_call 2)
  (func (;4;) (type 4) (param f64 f32 f32 i32 i64 i32 f32 f64 f64 i64 f32 i64 f32 i64 i64 i32 f64 f32 f64 f64 i64 f64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)
    local.get 0
    local.get 1
    local.get 4
    local.get 7
    local.get 3
    local.get 5
    local.get 2
    local.get 8
    local.get 15
    local.get 3
    local.get 9
    local.get 11
    local.get 13
    local.get 5
    local.get 15
    local.get 14
    local.get 16
    local.get 20
    local.get 18
    local.get 3
    local.get 19
    local.get 6
    i64.const 22
    f64.const 0x1.7p+4 (;=23;)
    f32.const 0x1.8p+4 (;=24;)
    f64.const 0x1.9p+4 (;=25;)
    f32.const 0x1.ap+4 (;=26;)
    f64.const 0x1.bp+4 (;=27;)
    i64.const 28
    i64.const 29
    f32.const 0x1.ep+4 (;=30;)
    f64.const 0x1.fp+4 (;=31;))
  (func (;5;) (type 5) (param f32 i64 f64 f32 i64 f64 f64 f64 f32 f32 f64 i64 i64 i64 i64 f64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)
    local.get 2
    local.get 0
    local.get 3
    i32.const 3
    local.get 1
    i32.const 5
    local.get 8
    local.get 5
    local.get 6
    local.get 4
    local.get 9
    local.get 11
    local.get 0
    local.get 12
    local.get 13
    i32.const 15
    f64.const 0x1p+4 (;=16;)
    f32.const 0x1.1p+4 (;=17;)
    f64.const 0x1.2p+4 (;=18;)
    f64.const 0x1.3p+4 (;=19;)
    i64.const 20
    f64.const 0x1.5p+4 (;=21;)
    return_call 4)
  (func (;6;) (type 6) (param i64 f32 i64 i64 f64 i64 f64 f64 f32 f64 f64 f32 f64 f64 i64 f64 f64 i32 f64 f64 f64 f32 f32 i64 i64 f32 f32 i32 i64 f64 i32 f64 i32 i32 i64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)
    local.get 1
    local.get 0
    local.get 4
    local.get 8
    local.get 2
    local.get 6
    local.get 7
    local.get 9
    local.get 11
    local.get 21
    local.get 10
    local.get 3
    local.get 5
    local.get 14
    local.get 23
    local.get 12
    return_call 5)
  (func (;7;) (type 7) (param f64 i32 i32 i64 f32 i64 i64 i64 f32 i32 i64 f32 f64 f64 i64) (result f64 f32 i64 f64 i32 i32 f32 f64 i32 i32 i64 i64 i64 i32 i32 i64 f64 i64 f64 i32 f64 f32 i64 f64 f32 f64 f32 f64 i64 i64 f32 f64)
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 0
    local.get 7
    local.get 12
    local.get 13
    local.get 8
    local.get 0
    local.get 12
    local.get 11
    local.get 13
    local.get 0
    local.get 10
    f64.const 0x1.ep+3 (;=15;)
    f64.const 0x1p+4 (;=16;)
    i32.const 17
    f64.const 0x1.2p+4 (;=18;)
    f64.const 0x1.3p+4 (;=19;)
    f64.const 0x1.4p+4 (;=20;)
    f32.const 0x1.5p+4 (;=21;)
    f32.const 0x1.6p+4 (;=22;)
    i64.const 23
    i64.const 24
    f32.const 0x1.9p+4 (;=25;)
    f32.const 0x1.ap+4 (;=26;)
    i32.const 27
    i64.const 28
    f64.const 0x1.dp+4 (;=29;)
    i32.const 30
    f64.const 0x1.fp+4 (;=31;)
    i32.const 32
    i32.const 33
    i64.const 34
    return_call 6)

  (func (;8;) (type 8) (param f32 i64 i64 f64 i32 f64 i64 i64 f32 f32 f32 f32 i32 i32 f32 i64 i32 f32 i64 f32 i64 i32 f32 f64 f32 f64 i64) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)
    local.get 4
    local.get 3
    local.get 5
    local.get 1
    local.get 2
    local.get 6
    local.get 0
    local.get 8
    local.get 12
    local.get 7
    local.get 15
    local.get 9
    local.get 10
    local.get 13
    local.get 11
    local.get 14
    local.get 17
    local.get 23
    local.get 25
    local.get 16
    local.get 19
    local.get 21
    local.get 22
    local.get 3
    local.get 4
    local.get 5
    local.get 24
    f64.const 0x1.bp+4 (;=27;)
    f32.const 0x1.cp+4 (;=28;)
    i32.const 29
    i32.const 30
    f64.const 0x1.fp+4 (;=31;)
    i64.const 32
    f64.const 0x1.08p+5 (;=33;)
    f32.const 0x1.1p+5 (;=34;)
    i64.const 35
    f64.const 0x1.2p+5 (;=36;))
  (func (;9;) (type 9) (param i32 f64 f32 i64 i32 i32 i32 i64 f32 i32 f64 i64 i32 f64 f64 i64 f64 i32 i32 i32 i64 f64 f32 f32 i64 f64 i32 i32 f32 f32) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)
    local.get 2
    local.get 3
    local.get 7
    local.get 1
    local.get 0
    local.get 10
    local.get 11
    local.get 15
    local.get 8
    local.get 22
    local.get 23
    local.get 28
    local.get 4
    local.get 5
    local.get 29
    local.get 20
    local.get 6
    local.get 2
    local.get 24
    local.get 8
    local.get 3
    local.get 9
    local.get 22
    local.get 13
    local.get 23
    local.get 14
    local.get 7
    return_call 8)
  (func (;10;) (type 10) (param f64 f32 f64 f64 i64 i32 f32 i32 f64 f64 i64 i64 i64 i64 i64 i64 i32 i64 i64 i32 f64 i64 f64 i64 i32 i64 i32 i32 f32 f64) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)
    local.get 5
    local.get 0
    local.get 1
    local.get 4
    local.get 7
    local.get 16
    local.get 19
    local.get 10
    local.get 6
    local.get 24
    local.get 2
    local.get 11
    local.get 26
    local.get 3
    local.get 8
    local.get 12
    local.get 9
    local.get 27
    local.get 5
    local.get 7
    local.get 13
    local.get 20
    local.get 28
    local.get 1
    local.get 14
    local.get 22
    local.get 16
    local.get 19
    local.get 6
    local.get 28
    return_call 9)
  (func (;11;) (type 11) (param i64 i64 f32 f32 f32 i64 i32 i64 f64 i32 i32) (result i32 f64 f64 i64 i64 i64 f32 f32 i32 i64 i64 f32 f32 i32 f32 f32 f32 f64 f64 i32 f32 i32 f32 f64 i32 f64 f32 f64 f32 i32 i32 f64 i64 f64 f32 i64 f64)
    local.get 8
    local.get 2
    local.get 8
    local.get 8
    local.get 0
    local.get 6
    local.get 3
    local.get 9
    local.get 8
    local.get 8
    local.get 1
    i64.const 11
    i64.const 12
    i64.const 13
    i64.const 14
    i64.const 15
    i32.const 16
    i64.const 17
    i64.const 18
    i32.const 19
    f64.const 0x1.4p+4 (;=20;)
    i64.const 21
    f64.const 0x1.6p+4 (;=22;)
    i64.const 23
    i32.const 24
    i64.const 25
    i32.const 26
    i32.const 27
    f32.const 0x1.cp+4 (;=28;)
    f64.const 0x1.dp+4 (;=29;)
    return_call 10)
  (func (;12;) (type 12) (param i32 f64 f64 f64 f32 i32 f32 f64 i32 i32 f32 i32 f64 i64 f32 i64 i32 i32 i32 f32 f32 f64 i32 i32 i64 i32 i32 f64 f32 i64 f64 f32) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)
    local.get 4
    local.get 0
    local.get 5
    local.get 1
    local.get 13
    local.get 2
    local.get 8
    local.get 3
    local.get 6
    local.get 15
    local.get 24
    local.get 29
    local.get 9
    local.get 11
    local.get 7
    local.get 12
    local.get 10
    local.get 13
    local.get 21
    local.get 14
    local.get 19
    local.get 27
    local.get 30
    local.get 16
    local.get 15
    local.get 20
    local.get 17
    local.get 28
    local.get 18
    local.get 31
    local.get 24
    local.get 22
    i64.const 32
    f32.const 0x1.08p+5 (;=33;)
    f64.const 0x1.1p+5 (;=34;)
    i32.const 35
    f32.const 0x1.2p+5 (;=36;))
  (func (;13;) (type 13) (param i32 f64 i64 f64 f64 f32 f32 f32 i64 i32 i64 f32 i64 f32 f32 i32 f64 i32 i64 f32 f64 f32 i32 i32 f32 i64 f32) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)
    local.get 0
    local.get 1
    local.get 3
    local.get 4
    local.get 5
    local.get 9
    local.get 6
    local.get 16
    local.get 15
    local.get 17
    local.get 7
    local.get 22
    local.get 20
    local.get 2
    local.get 11
    local.get 8
    local.get 23
    local.get 0
    local.get 9
    local.get 13
    local.get 14
    local.get 1
    local.get 15
    local.get 17
    local.get 10
    local.get 22
    local.get 23
    f64.const 0x1.bp+4 (;=27;)
    f32.const 0x1.cp+4 (;=28;)
    i64.const 29
    f64.const 0x1.ep+4 (;=30;)
    f32.const 0x1.fp+4 (;=31;)
    i32.const 12
    return_call_indirect (type 12))
  (func (;14;) (type 14) (param f64 i64 f64 i64 f64 f64 i64 i32 f32 f32 i64 i32 f32 i32 i32 i32 f64 i32 i64 i32 i32 f64 f32 f64 i64 i64 f64 i32) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)
    local.get 7
    local.get 0
    local.get 1
    local.get 2
    local.get 4
    local.get 8
    local.get 9
    local.get 12
    local.get 3
    local.get 11
    local.get 6
    local.get 22
    local.get 10
    local.get 8
    local.get 9
    local.get 13
    local.get 5
    local.get 14
    local.get 18
    local.get 12
    local.get 16
    local.get 22
    local.get 15
    local.get 17
    local.get 8
    local.get 24
    local.get 9
    i32.const 13
    return_call_indirect (type 13))
  (func (;15;) (type 15) (param i32 f32 f32 i32 f64 i32 i64 f64 i32 i32 i32 i64) (result f32 i32 i32 f64 i64 f64 i32 f64 f32 i64 i64 i64 i32 i32 f64 f64 f32 i64 f64 f32 f32 f64 f64 i32 i64 f32 i32 f32 i32 f32 i64 i32 i64 f32 f64 i32 f32)
    local.get 4
    local.get 6
    local.get 7
    local.get 11
    local.get 4
    local.get 7
    local.get 6
    local.get 0
    local.get 1
    local.get 2
    local.get 11
    local.get 3
    f32.const 0x1.8p+3 (;=12;)
    i32.const 13
    i32.const 14
    i32.const 15
    f64.const 0x1p+4 (;=16;)
    i32.const 17
    i64.const 18
    i32.const 19
    i32.const 20
    f64.const 0x1.5p+4 (;=21;)
    f32.const 0x1.6p+4 (;=22;)
    f64.const 0x1.7p+4 (;=23;)
    i64.const 24
    i64.const 25
    f64.const 0x1.ap+4 (;=26;)
    i32.const 27
    i32.const 14
    return_call_indirect (type 14))
  (func (;16;) (type 16) (param i64 i64 i64 i64 f32 f32 f64 f32 i32 i32 f32 i64 i64 f64 i32 i64 i64 f64) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)
    local.get 0
    local.get 4
    local.get 1
    local.get 8
    local.get 2
    local.get 6
    local.get 9
    local.get 5
    local.get 7
    local.get 14
    local.get 8
    local.get 9
    local.get 10)
  (func (;17;) (type 17) (param f32 f64 f32 f64 f64 i32 f32 f64 i32 f32 i64 f64 i64 i64 i32 i64 i32 f64 i64 i32 i32 i32 f64 f32 f64 i64 i32 f32) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)
    local.get 10
    local.get 12
    local.get 13
    local.get 15
    local.get 0
    local.get 2
    local.get 1
    local.get 6
    local.get 5
    local.get 8
    local.get 9
    local.get 18
    local.get 25
    local.get 3
    local.get 14
    local.get 10
    local.get 12
    local.get 4
    i32.const 16
    return_call_indirect (type 16))
  (func (;18;) (type 18) (param i32 f32 i64 i32 i64 f32 f64 f64 f64 i32 i64 f64 i32 i64 i64 i32 i32 f64 i64 i32 f64 i32 i64 i64 f32 f64 i64 i32 f64 f32 f32 i64 f64 f64 f32 i32 f64) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)
    local.get 1
    local.get 6
    local.get 5
    local.get 7
    local.get 8
    local.get 0
    local.get 24
    local.get 11
    local.get 3
    local.get 29
    local.get 2
    local.get 17
    local.get 4
    local.get 10
    local.get 9
    local.get 13
    local.get 12
    local.get 20
    local.get 14
    local.get 15
    local.get 16
    local.get 19
    local.get 25
    local.get 30
    local.get 28
    local.get 18
    local.get 21
    local.get 34
    i32.const 17
    return_call_indirect (type 17))
  (func (;19;) (type 19) (param f64 i32 f32 f32 i32 i64 i32 i32 i32 f64 i64 i32 i64 f64 f64 i32 f64 i64 f64 f64) (result i64 f32 i64 i32 i64 f64 i32 f32 f32 i32 i32 i32 f32)
    local.get 1
    local.get 2
    local.get 5
    local.get 4
    local.get 10
    local.get 3
    local.get 0
    local.get 9
    local.get 13
    local.get 6
    local.get 12
    local.get 14
    local.get 7
    local.get 17
    local.get 5
    local.get 8
    local.get 11
    local.get 16
    local.get 10
    local.get 15
    f64.const 0x1.4p+4 (;=20;)
    i32.const 21
    i64.const 22
    i64.const 23
    f32.const 0x1.8p+4 (;=24;)
    f64.const 0x1.9p+4 (;=25;)
    i64.const 26
    i32.const 27
    f64.const 0x1.cp+4 (;=28;)
    f32.const 0x1.dp+4 (;=29;)
    f32.const 0x1.ep+4 (;=30;)
    i64.const 31
    f64.const 0x1p+5 (;=32;)
    f64.const 0x1.08p+5 (;=33;)
    f32.const 0x1.1p+5 (;=34;)
    i32.const 35
    f64.const 0x1.2p+5 (;=36;)
    i32.const 18
    return_call_indirect (type 18))
  (func (;20;) (type 20) (param i32 f32 f32 f32 f64 i64 f64 f32 i64 i64 i32 f64 i64 i32 f64 f32 f32 i32 i64 f32 i64 i64 f32 f32 f64 i64 f64 f64 i32 f64 f64 f32 f64 i32 f64 f32 i32 i64 i32 f64) (result f32 f32 i32 i64 f32)
    local.get 1
    local.get 2
    local.get 0
    local.get 5
    local.get 3)
  (func (;21;) (type 21) (param f32 i32 i64 f32 i32 i64 i64 i64 i32 i32 i32 i64 f64 f32 i32 f64 f32 f32 f32 f64 f64 i32 f64 i32 i64 i64 f64 i64 i64 f64 i64 i64 f64 i64) (result f32 f32 i32 i64 f32)
    local.get 1
    local.get 0
    local.get 3
    local.get 13
    local.get 12
    local.get 2
    local.get 15
    local.get 16
    local.get 5
    local.get 6
    local.get 4
    local.get 19
    local.get 7
    local.get 8
    local.get 20
    local.get 17
    local.get 18
    local.get 9
    local.get 11
    local.get 0
    local.get 24
    local.get 25
    local.get 3
    local.get 13
    local.get 22
    local.get 27
    local.get 26
    local.get 29
    local.get 10
    local.get 32
    local.get 12
    local.get 16
    local.get 15
    local.get 14
    f64.const 0x1.1p+5 (;=34;)
    f32.const 0x1.18p+5 (;=35;)
    i32.const 36
    i64.const 37
    i32.const 38
    f64.const 0x1.38p+5 (;=39;)
    i32.const 20
    return_call_indirect (type 20))
  (func (;22;) (type 22) (param f32 f32 i32 i32 f32 i32 f64 i64 f32 i32 i64 i64 i64 i32 f32 f32 i32 f32 f64 f32 i32 i32 f32 i64 i32 f64 f32 i64 i32 i32 i32 i64) (result f32 f32 i32 i64 f32)
    local.get 0
    local.get 2
    local.get 7
    local.get 1
    local.get 3
    local.get 10
    local.get 11
    local.get 12
    local.get 5
    local.get 9
    local.get 13
    local.get 23
    local.get 6
    local.get 4
    local.get 16
    local.get 18
    local.get 8
    local.get 14
    local.get 15
    local.get 25
    local.get 6
    local.get 20
    local.get 18
    local.get 21
    local.get 27
    local.get 31
    local.get 25
    local.get 7
    local.get 10
    local.get 6
    local.get 11
    local.get 12
    f64.const 0x1p+5 (;=32;)
    i64.const 33
    i32.const 21
    return_call_indirect (type 21))
  (func (;23;) (type 23) (param f32 f64 f64 f64 f32 f64 f32 i32 i32 i64 f64 i32 f32 f64 i64 f32 i64) (result f32 f32 i32 i64 f32)
    local.get 0
    local.get 4
    local.get 7
    local.get 8
    local.get 6
    local.get 11
    local.get 1
    local.get 9
    local.get 12
    local.get 7
    local.get 14
    local.get 16
    local.get 9
    local.get 8
    local.get 15
    local.get 0
    local.get 11
    f32.const 0x1.1p+4 (;=17;)
    f64.const 0x1.2p+4 (;=18;)
    f32.const 0x1.3p+4 (;=19;)
    i32.const 20
    i32.const 21
    f32.const 0x1.6p+4 (;=22;)
    i64.const 23
    i32.const 24
    f64.const 0x1.9p+4 (;=25;)
    f32.const 0x1.ap+4 (;=26;)
    i64.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i64.const 31
    i32.const 22
    return_call_indirect (type 22))
  (table (;0;) 24 funcref)
  (export "tail_0" (func 0))
  (export "depth0->tail_0" (func 1))
  (export "depth1->depth0->tail_0" (func 2))
  (export "depth2->depth1->depth0->tail_0" (func 3))
  (export "tail_4" (func 4))
  (export "depth0->tail_4" (func 5))
  (export "depth1->depth0->tail_4" (func 6))
  (export "depth2->depth1->depth0->tail_4" (func 7))

  (export "tail_8" (func 8))
  (export "depth0->tail_8" (func 9))
  (export "depth1->depth0->tail_8" (func 10))
  (export "depth2->depth1->depth0->tail_8" (func 11))

  (export "tail_12" (func 12))
  (export "depth0indirect->tail_12" (func 13))
  (export "depth1indirect->depth0indirect->tail_12" (func 14))
  (export "depth2indirect->depth1indirect->depth0indirect->tail_12" (func 15))
  (export "tail_16" (func 16))
  (export "depth0indirect->tail_16" (func 17))
  (export "depth1indirect->depth0indirect->tail_16" (func 18))
  (export "depth2indirect->depth1indirect->depth0indirect->tail_16" (func 19))
  (export "tail_20" (func 20))
  (export "depth0indirect->tail_20" (func 21))
  (export "depth1indirect->depth0indirect->tail_20" (func 22))
  (export "depth2indirect->depth1indirect->depth0indirect->tail_20" (func 23))
  (elem (;0;) (i32.const 0) func 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23))
`

async function test() {
    const instance = await instantiate(wat, {}, { tail_call: true });
    assert.eq(instance.exports['depth2->depth1->depth0->tail_8'](1n, 2n, 3, 4, 5, 6n, 7, 8n, 9, 10, 11), [7,9,9,1n,2n,11n,3,4,10,12n,13n,28,3,16,4,28,3,9,9,19,4,24,28,9,7,9,3,27,28,29,30,31,32n,33,34,35n,36]);
}

await assert.asyncTest(test())
