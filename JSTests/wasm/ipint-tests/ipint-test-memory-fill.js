import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (func (export "fill") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (memory.fill)
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
    const { memory, fill } = instance.exports
    fill(0, 65, 16);
    assert.eq(decodeString(memory.buffer), "AAAAAAAAAAAAAAAA");
}

assert.asyncTest(test())
