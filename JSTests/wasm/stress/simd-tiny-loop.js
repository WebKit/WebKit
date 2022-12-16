//@ requireOptions("--useWebAssemblySIMD=1", "--watchdog=1000", "--watchdog-exception-ok")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
//@ skip
//FIXME: this test is currently broken.
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test") (;0;)
        (local v128)
        loop (result v128)  ;; label = @1
        br 0 (;@1;)
        end
        local.set 0
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        test()
    }
}

assert.asyncTest(test())
