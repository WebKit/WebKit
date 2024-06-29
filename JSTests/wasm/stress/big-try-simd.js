//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const N = 10

let wat = `
(module
    (tag $t0 (param `

for (let i = 0; i < N; ++i)
    wat += `v128 `

wat += `))

    (func $f0 `
for (let i = 0; i < 50; ++i)
    wat += `(local v128) `
for (let i = 0; i < N; ++i)
    wat += `(local $l${i} v128) `

for (let i = 0; i < N; ++i)
    wat += `(local.set $l${i} (v128.const i64x2 0 ${i + 5})) `


for (let i = 0; i < N; ++i)
    wat += `(local.get $l${i}) `

wat += `
        (throw $t0)
    )
    (func $f1 (export "test")
      (try
        (do
            (call $f0))
        (catch $t0
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

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(42), undefined);
    }
}

await assert.asyncTest(test())
