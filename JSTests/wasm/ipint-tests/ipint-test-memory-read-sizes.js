import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (func (export "i32") (result i32)
        (i32.const 0)
        (i32.load)
    )
    (func (export "i32_8s") (result i32)
        (i32.const 0)
        (i32.load8_s)
    )
    (func (export "i32_8u") (result i32)
        (i32.const 0)
        (i32.load8_u)
    )
    (func (export "i32_16s") (result i32)
        (i32.const 0)
        (i32.load16_s)
    )
    (func (export "i32_16u") (result i32)
        (i32.const 0)
        (i32.load16_u)
    )

    (func (export "i64") (result i64)
        (i32.const 0)
        (i64.load)
    )
    (func (export "i64_8s") (result i64)
        (i32.const 0)
        (i64.load8_s)
    )
    (func (export "i64_8u") (result i64)
        (i32.const 0)
        (i64.load8_u)
    )
    (func (export "i64_16s") (result i64)
        (i32.const 0)
        (i64.load16_s)
    )
    (func (export "i64_16u") (result i64)
        (i32.const 0)
        (i64.load16_u)
    )
    (func (export "i64_32s") (result i64)
        (i32.const 0)
        (i64.load32_s)
    )
    (func (export "i64_32u") (result i64)
        (i32.const 0)
        (i64.load32_u)
    )
)
`

function writeToMemory(m, l) {
    let arr = new Uint8Array(m.buffer);
    for (let i = 0; i < l; ++i) {
        arr[i] = 128 + i;
    }
}

async function test() {
    const instance = await instantiate(wat, {});
    const { memory, 
        i32, i32_8s, i32_8u, i32_16s, i32_16u, 
        i64, i64_8s, i64_8u, i64_16s, i64_16u, i64_32s, i64_32u
    } = instance.exports
    writeToMemory(memory, 16);

    // mem = 80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f

    // 0x83828180 as 2s complement
    assert.eq(i32(), -2088599168);
    // 0x80 as 2s complement
    assert.eq(i32_8s(), -128);
    assert.eq(i32_8u(), 128);
    // 0x8180 as 2s complement
    assert.eq(i32_16s(), -32384);
    assert.eq(i32_16u(), 33152);

    // 0x8786858483828180 as 2s complement
    assert.eq(i64(), -8681104427521506944n);
    // 0x80 as 2s complement
    assert.eq(i64_8s(), -128n);
    assert.eq(i64_8u(), 128n);
    // 0x8180 as 2s complement
    assert.eq(i64_16s(), -32384n);
    assert.eq(i64_16u(), 33152n);
    // 0x83828180 as 2s complement
    assert.eq(i64_32s(), -2088599168n);
    assert.eq(i64_32u(), 2206368128n);
}

assert.asyncTest(test())
