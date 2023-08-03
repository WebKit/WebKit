import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (data "the option is whether or not to let failure be the last thing you do.")
    (func (export "init") (param i32 i32 i32)
        (local.get 0)
        (local.get 1)
        (local.get 2)
        (memory.init 0)
    )
    (func (export "yeet")
        (data.drop 0)
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
    const { memory, init, yeet } = instance.exports
    init(0, 0, 69);
    assert.eq(decodeString(memory.buffer), "the option is whether or not to let failure be the last thing you do.");
    yeet();
    assert.throws(() => {init(0, 0, 5)}, WebAssembly.RuntimeError, "Out of bounds memory access (evaluating 'init(0, 0, 5)')");
}

assert.asyncTest(test())
