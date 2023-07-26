import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (data (i32.const 0x0) "\x0e\xec\x53\x88")
    (func (export "test")
        (i32.const 0)
        (i32.const 1717661556)
        (i32.store)
        (i32.const 4)
        (i32.const 117)
        (i32.store)
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
    const { memory, test } = instance.exports
    test();
    assert.eq(decodeString(memory.buffer), "toafu");
}

assert.asyncTest(test())
