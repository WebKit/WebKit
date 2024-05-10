import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (global $0 f32 f32.const 42.0)
    (func (export "test") (local i32 i32)
        local.get 0
        if
            block (result i32 i32)
                local.get 0
                local.get 1
            end
            unreachable
        else
            global.get $0
            f32.const 42.0
            f32.eq
            br_if 1
            unreachable
        end
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { test } = instance.exports;
    test();
}

await assert.asyncTest(test())
