import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test_i32") (local i32 i32)
        i32.const 42
        local.set 1

        local.get 0 ;; 32 locals should require all GPRs on all platforms
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 1 ;; here's the value we actually care about

        block (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
            i32.const 1
            i32.add
            br 0
        end

        i32.const 43
        i32.eq
        br_if 0
        unreachable
    )

    (func (export "test_f32") (local f32 f32)
        f32.const 42.0
        local.set 1

        local.get 0 ;; 32 locals should require all FPRs on all platforms
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 0
        local.get 1 ;; here's the value we actually care about

        block (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)
            f32.neg
            br 0
        end

        f32.const -42.0
        f32.eq
        br_if 0
        unreachable
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { test_i32, test_f32 } = instance.exports;
    test_i32();
    test_f32();
}

await assert.asyncTest(test())
