import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "test")
        i64.const 0
        i32.const 0

        (if (param i64)
            (then
            drop
            
            f32.const 0
            drop
            )
            (else drop)
        )
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
