import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (data "doing it for the bit since 2003")
    (func (export "init") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (memory.init 0)
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
    const { memory, init } = instance.exports
    init(0, 0, 20);
    assert.eq(decodeString(memory.buffer), "doing it for the bit");
    init(13, 0, 31);
    assert.eq(decodeString(memory.buffer), "doing it for doing it for the bit since 2003");
}

assert.asyncTest(test())
