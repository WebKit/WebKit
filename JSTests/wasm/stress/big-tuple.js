//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const N = 1000

let wat = `
(module
    (func $f0 (result `

for (let i = 0; i < N; ++i)
    wat += `f64 `

wat += `) (local $l0 f64) `

for (let i = 0; i < N; ++i)
  wat += `(local.get $l0) `

wat += `
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
