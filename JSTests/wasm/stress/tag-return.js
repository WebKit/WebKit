//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const N = 15

let wat = `
(module
    (tag $t0 (result f32)))`

async function test() {
    await assert.throwsAsync(instantiate(wat, {}, { exceptions: true }), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 20: 0th Exception type cannot have a non-void return type 0 (evaluating 'new WebAssembly.Module(binaryResult.buffer)')")
}

assert.asyncTest(test())
