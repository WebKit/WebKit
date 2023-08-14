import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $add (export "add") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        (local.get 0)
        (local.get 1)
        (i32.add)
        (local.get 2)
        (i32.add)
        (local.get 3)
        (i32.add)
        (local.get 4)
        (i32.add)
        (local.get 5)
        (i32.add)
        (local.get 6)
        (i32.add)
        (local.get 7)
        (i32.add)
        (local.get 8)
        (i32.add)
        (local.get 9)
        (i32.add)
        (local.get 10)
        (i32.add)
        (local.get 11)
        (i32.add)
        (return)
    )
)
`

let seed = 89666917;
function prng() {
    // xorshift
    let v = seed;
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return v % 256;
}

async function test() {
    const instance = await instantiate(wat, {});
    const { test, add } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        let values = new Array(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).map((_) => prng());
        assert.eq(add(...values), values.reduce((p, c) => p + c, 0));
    }
}

assert.asyncTest(test())
