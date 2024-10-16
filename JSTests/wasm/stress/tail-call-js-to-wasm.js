import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let count = 10;
function splat(strOrGen, num = count) {
    let result = Array(num).fill(strOrGen); // need to fill the array otherwise map yields nothing.
    if (typeof strOrGen == "function")
        result = result.map((_, i) => strOrGen(i));
    return result.join(" ");
}

async function testTail() {
    let wat = `
    (module
        (func (export "test") (result ${splat("i32")})
            (return_call $tail ${splat((i) => "(i32.const " + i + ")\n", count * 2)})
        )

        (func $tail (param ${splat("i32", count * 2)}) (result ${splat("i32")})
            ${splat((i) => "(local.get " + (i) + ")\n")}
        )
    )
    `

    let instance = await instantiate(wat, {}, { tail_call: true });
    let func = instance.exports.test;

    for (let i = 0; i < 1e4; ++i) {
        let results = func();
        for (let j = 0; j < count; ++j)
            assert.eq(results[j], j);
    }
}

async function testCallToTail() {
    let wat = `
    (module
        (func (export "test") (result ${splat("i32")})
            (call $callee ${splat((i) => "(i32.const " + i + ")\n")})
        )
        (func $callee (param ${splat("i32")}) (result ${splat("i32")})
            ${splat((i) => "(local.get " + (i % count) + ")\n", count * 2)}
            (return_call $tail)
        )

        (func $tail (param ${splat("i32", count * 2)}) (result ${splat("i32")})
            ${splat((i) => "(local.get " + (i) + ")\n")}
        )
    )
    `

    let instance = await instantiate(wat, {}, { tail_call: true });
    let func = instance.exports.test;

    for (let i = 0; i < 1e4; ++i) {
        let results = func();
        for (let j = 0; j < count; ++j)
            assert.eq(results[j], j);
    }
}

assert.asyncTest(testTail());
assert.asyncTest(testCallToTail());
