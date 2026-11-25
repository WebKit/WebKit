//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { watToWasm } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const N = 15

let wat = `
(module
    (tag $t0 (result f32)))`

async function test() {
    await assert.compileErrorAsync(watToWasm(wat, { exceptions: true }), "WebAssembly.Module doesn't parse at byte 20: 0th Exception type cannot have a non-void return type 0")
}

await assert.asyncTest(test())
