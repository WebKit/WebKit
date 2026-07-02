//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform

import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (import "env" "target" (global externref))

    (global $g1 v128 (v128.const i64x2 0 0))
    (global $g2 v128 (global.get $g1))

    (func (export "readG2") (result i64)
        (i64x2.extract_lane 0
            (global.get $g2)
        )
    )
)
`

async function test() {
    const instance = await instantiate(wat, { env: { target: {} } }, { simd: true });
    assert.eq(instance.exports.readG2().toString(16), "0");
}

await assert.asyncTest(test());
