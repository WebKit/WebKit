import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

function sum32(...args) {
    let total = 0
    for (let value of args)
        total += value
    return total
}

async function test() {
    const values = Array.from({ length: 32 }, (_, i) => i + 1)
    let expected = 0
    for (let value of values)
        expected += value

    const i32Params = Array(32).fill("i32").join(" ")
    const i32Gets = Array.from({ length: 32 }, (_, i) => `(local.get ${i})`).join("\n        ")
    const i32Wat = `
    (module
        (import "m" "sum" (func $sum (param ${i32Params}) (result i32)))
        (func (export "call") (param ${i32Params}) (result i32)
            ${i32Gets}
            call $sum)
    )`

    const { call } = (await instantiate(i32Wat, { m: { sum: sum32 } })).exports
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(call(...values), expected)

    const mixedParams = [...Array(16).fill("i32"), ...Array(16).fill("f64")].join(" ")
    const mixedGets = Array.from({ length: 32 }, (_, i) => `(local.get ${i})`).join("\n        ")
    const mixedWat = `
    (module
        (import "m" "sum" (func $sum (param ${mixedParams}) (result f64)))
        (func (export "call") (param ${mixedParams}) (result f64)
            ${mixedGets}
            call $sum)
    )`

    const mixedValues = [...values.slice(0, 16), ...values.slice(0, 16)]
    let mixedExpected = 0
    for (let value of mixedValues)
        mixedExpected += value
    const mixed = (await instantiate(mixedWat, { m: { sum: sum32 } })).exports
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(mixed.call(...mixedValues), mixedExpected)
}

await assert.asyncTest(test())
