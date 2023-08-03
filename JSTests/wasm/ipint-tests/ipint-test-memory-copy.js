import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (data (i32.const 0x0) "one ")
    (func (export "copy") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (memory.copy)
    )
)
`

function decodeString(buffer) {
    let s = "";
    let i = 0;
    let arr = new Uint8Array(buffer);
    while (arr[i] != 0) {
        s += String.fromCharCode(arr[i++]);
    }
    return s;
}

async function test() {
    const instance = await instantiate(wat, {});
    const { memory, copy } = instance.exports
    assert.eq(decodeString(memory.buffer), "one ");
    copy(4, 0, 4);
    assert.eq(decodeString(memory.buffer), "one one ");
}

assert.asyncTest(test())
