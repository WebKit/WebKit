import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test_i32")
        block (result i32)
            block (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
                i32.const 1 ;; 32 values should be sufficient to require stack arguments on all platforms
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 1
                i32.const 42 ;; here's the value we actually care about
            end
            i32.const 1
            i32.add
            br 0
        end

        i32.const 43
        i32.eq
        br_if 0
        unreachable
    )

    (func (export "test_f32")
        block (result f32)
            block (result f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32)
                f32.const 1.0 ;; 32 values should be sufficient to require stack arguments on all platforms
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 1.0
                f32.const 42.0 ;; here's the value we actually care about
            end
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
