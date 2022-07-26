import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "bitmask") (result i32)
        (v128.const i64x2 -13 5)
        (i64x2.bitmask)
        
        (return)
    )
)
`

async function test() {
    const instance = await instantiate(wat, { imports: { } }, { simd: true })
    
    const { 
        bitmask,
    } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(bitmask(), 0b010)
    }
}

assert.asyncTest(test())
