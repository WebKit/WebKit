//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $f0 (result v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128 v128)
      (local $l0 v128)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
      (local.get $l0)
    )
    (func $f1 (export "test")
      (call $f0)
      (return))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), undefined)
    }
}

assert.asyncTest(test())
