//@ requireOptions("--useWasmSIMD=1", "--useWasmTailCalls=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// rdar://174490087

let wat = `
(module
  (func $callee
    (param i64 i64 i64 i64 i64 i64 i64 i64)          ;; 0-7   GPR
    (param v128 v128 v128 v128 v128 v128 v128 v128)   ;; 8-15  FPR
    (param i64)                                        ;; 16    first stack param
    (param v128)                                       ;; 17    v128
    (param externref)                                  ;; 18    externref overlapping v128
    (param i64 i64 i64 i64 i64 i64 i64 i64 i64 i64
           i64 i64 i64 i64 i64 i64 i64 i64 i64 i64
           i64 i64 i64 i64 i64 i64 i64 i64 i64 i64
           i64 i64 i64 i64 i64 i64 i64 i64 i64 i64
           i64 i64 i64 i64 i64 i64)                   ;; 19-64 padding
    (result externref)
    local.get 18)

  (func $caller (export "caller") (param $r externref) (result externref)
    (local $z i64)
    ;; GPR params 0-7
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    ;; FPR params 8-15
    v128.const i64x2 0 0  v128.const i64x2 0 0
    v128.const i64x2 0 0  v128.const i64x2 0 0
    v128.const i64x2 0 0  v128.const i64x2 0 0
    v128.const i64x2 0 0  v128.const i64x2 0 0
    ;; param 16: i64
    i64.const 0
    ;; param 17: v128
    v128.const i64x2 0x1111111111111111 0x2222222222222222
    ;; param 18: the externref
    local.get $r
    ;; params 19-64: padding to force spills
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z local.get $z local.get $z local.get $z local.get $z
    local.get $z
    return_call $callee))
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, tail_call: true, exceptions: true })
    const { caller } = instance.exports
    const sentinel = { marker: 0x1337 }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(caller(sentinel), sentinel)
    }
}

await assert.asyncTest(test())
