import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1 10)
    (func (export "i32") (param i32 i32)
        (local.get 0)
        (local.get 1)
        (i32.store)
    )
    (func (export "i32_8") (param i32 i32)
        (local.get 0)
        (local.get 1)
        (i32.store8)
    )
    (func (export "i32_16") (param i32 i32)
        (local.get 0)
        (local.get 1)
        (i32.store16)
    )

    (func (export "i64") (param i32 i64)
        (local.get 0)
        (local.get 1)
        (i64.store)
    )
    (func (export "i64_8") (param i32 i64)
        (local.get 0)
        (local.get 1)
        (i64.store8)
    )
    (func (export "i64_16") (param i32 i64)
        (local.get 0)
        (local.get 1)
        (i64.store16)
    )
    (func (export "i64_32") (param i32 i64)
        (local.get 0)
        (local.get 1)
        (i64.store32)
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
    const { memory, 
        i32, i32_8, i32_16,
        i64, i64_8, i64_16, i64_32
    } = instance.exports
    let writeString = (s) => {
        for (let i = 0; i < s.length; ++i) {
            i32_8(i, s.charCodeAt(i));
        }
    };

    i32(0, 1953719668);
    assert.eq(decodeString(memory.buffer), "test");

    i64(0, 8315168158489994617n);
    assert.eq(decodeString(memory.buffer), "yeonbies");

    i32(0, 2003789165);
    assert.eq(decodeString(memory.buffer), "meowbies");

    i32_16(0, 65);
    assert.eq(decodeString(memory.buffer), "A");

    i32_8(0, 26214);
    assert.eq(decodeString(memory.buffer), "f");

    i64(0, 0n);

    i64_8(0, 8315168158640989549n);
    assert.eq(decodeString(memory.buffer), "m");

    i64_16(0, 8315168158640989549n);
    assert.eq(decodeString(memory.buffer), "me");

    i64_32(0, 8315168158640989549n);
    assert.eq(decodeString(memory.buffer), "meow");

    i64(0, 8315168158640989549n);
    assert.eq(decodeString(memory.buffer), "meowbies");
}

assert.asyncTest(test())
