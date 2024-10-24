//@ requireOptions("--useWasmSIMD=1", "--useWasmTailCalls=true", "--maximumWasmCalleeSIzeForInlining=0")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $start (export "start")
      (call $f1))
    (func $f1
      (local v128)
      (try
        (do)
        (catch_all))
      (return_call $f2))
    (func $f2)
  )
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true, tail_call: true, exceptions: true })
    const { start } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        start()
    }
}

await assert.asyncTest(test())
