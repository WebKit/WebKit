//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform or $buildType == "debug"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const N = 1000

let wat = `
(module
    (tag $t0 (param `

for (let i = 0; i < N; ++i)
    wat += `f64 `

wat += `))

    (func $f0 `
for (let i = 0; i < 5000; ++i)
    wat += `(local f64) `
for (let i = 0; i < N; ++i)
    wat += `(local $l${i} f64) `

for (let i = 0; i < N; ++i)
    wat += `(local.set $l${i} (f64.const ${i + 5})) `


for (let i = 0; i < N; ++i)
    wat += `(local.get $l${i}) `

wat += `
        (throw $t0)
    )
    (func $f1 (export "test") (result f64)
      (try
        (do
            (call $f0))
        (catch $t0
`
for (let i = 0; i < N - 2; ++i)
    wat += `(drop) `

wat += `
            (f64.add)
            (return)
        )
      )
      (unreachable)
      (return))
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, exceptions: true })
    const { test } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(test(42), 2*5 + 0 + 1)
    }
}

await assert.asyncTest(test())
