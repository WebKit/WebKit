//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

async function test() {
    let wat = `(module
      (func $bad (export "bad") (result i64)
        (local $l41 v128)
        (local.set $l41
        (v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF))

          (if (result i64)
            (i32.eqz
              (v128.any_true
                (local.get $l41)))
            (then (i64.const 1))
            (else
              (i64x2.extract_lane 1
                  (v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF))
            )
          )
      )
)
`
    const instance = await instantiate(wat, { }, { simd: true, log: false })
    const { bad } = instance.exports

    for (let i = 0; i < 100000; ++i) {
      assert.eq(bad(), -1n)
    }
}

await assert.asyncTest(test())
