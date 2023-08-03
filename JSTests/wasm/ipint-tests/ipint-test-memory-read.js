import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (data (i32.const 0x0) "\x0e\xec\x53\x88")
    (func (export "test") (param i32) (result i32)
        (local.get 0)
        (i32.load8_u)
        (return)
    )
)
`

function writeStringToMemory(s, m) {
    let arr = new Uint8Array(m.buffer);
    for (let i = 0; i < s.length; ++i) {
        arr[i] = s.charCodeAt(i);
    }
}

async function test() {
    const instance = await instantiate(wat, {});
    const { memory, test } = instance.exports
    writeStringToMemory("shy little frog (@nimonyx)", memory);
    assert.eq(test(0), 115);
    assert.eq(test(10), 32);
    assert.eq(test(17), 64);
}

assert.asyncTest(test())
