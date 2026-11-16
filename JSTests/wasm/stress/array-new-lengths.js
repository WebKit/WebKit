//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// array.new fills its elements the same way array.fill does: a few overlapping stores for short
// lengths, a store loop for longer ones, and a call for the longest. Sweep lengths across all of
// those for every element type, with both a runtime and a constant length.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const byteLength = 640;
const constantLengths = [0, 1, 3, 7, 17, 33, 100];

const cases = [
    { type: "i8", valueType: "i32", size: 1, get: "array.get_u", eq: "i32.eq", first: "(i32.const 0x5b)", second: "(i32.const 0x7f)" },
    { type: "i16", valueType: "i32", size: 2, get: "array.get_u", eq: "i32.eq", first: "(i32.const 0x1234)", second: "(i32.const 0x7777)" },
    { type: "i32", valueType: "i32", size: 4, get: "array.get", eq: "i32.eq", first: "(i32.const 0x12345678)", second: "(i32.const 0x5a5a5a5a)" },
    { type: "f32", valueType: "f32", size: 4, get: "array.get", eq: "f32.eq", first: "(f32.const 1.5)", second: "(f32.reinterpret_i32 (i32.const 0x41414141))" },
    { type: "i64", valueType: "i64", size: 8, get: "array.get", eq: "i64.eq", first: "(i64.const 0x123456789abcdef0)", second: "(i64.const 0x5a5a5a5a5a5a5a5a)" },
    { type: "f64", valueType: "f64", size: 8, get: "array.get", eq: "f64.eq", first: "(f64.const 1.5)", second: "(f64.reinterpret_i64 (i64.const 0x4141414141414141))" },
    { type: "(ref null $s)", valueType: "(ref null $s)", size: 8, get: "array.get", eq: "ref.eq", first: "(global.get $first)", second: "(global.get $second)" },
];

function lengths(max) {
    let result = [];
    for (let length = 0; length <= Math.min(max, 33); ++length)
        result.push(length);
    for (let length = 45; length < max; length += 19)
        result.push(length);
    result.push(max);
    return result;
}

async function testArray({ type, valueType, size, get, eq, first, second }) {
    const maxLength = byteLength / size;
    const constantTests = constantLengths.map(length => `
            (func (export "constant${length}") (result i32)
                (call $check (array.new $arr ${first} (i32.const ${length})) (i32.const ${length}) (i32.const 1)))`).join("");
    const exports = (await instantiate(`
        (module
            (type $s (struct (field i32)))
            (type $arr (array (mut ${type})))
            (global $first (ref null $s) (struct.new $s (i32.const 1)))
            (global $second (ref null $s) (struct.new $s (i32.const 2)))

            (func $value (param $which i32) (result ${valueType})
                (if (result ${valueType}) (local.get $which) (then ${first}) (else ${second})))

            (func $check (param $array (ref $arr)) (param $length i32) (param $which i32) (result i32)
                (local $i i32)
                (if (i32.ne (array.len (local.get $array)) (local.get $length))
                    (then (return (i32.const -1))))
                (block $done
                    (br_if $done (i32.eqz (local.get $length)))
                    (loop $loop
                        (if (i32.eqz (${eq} (${get} $arr (local.get $array) (local.get $i)) (call $value (local.get $which))))
                            (then (return (i32.add (local.get $i) (i32.const 1)))))
                        (local.set $i (i32.add (local.get $i) (i32.const 1)))
                        (br_if $loop (i32.lt_u (local.get $i) (local.get $length)))))
                (i32.const 0))

            (func (export "variable") (param $length i32) (param $which i32) (result i32)
                (call $check (array.new $arr (call $value (local.get $which)) (local.get $length)) (local.get $length) (local.get $which)))
            (func (export "variableFirst") (param $length i32) (result i32)
                (call $check (array.new $arr ${first} (local.get $length)) (local.get $length) (i32.const 1)))
            ${constantTests})
    `)).exports;

    for (let length of lengths(maxLength)) {
        for (let which of [0, 1])
            assert.eq(exports.variable(length, which), 0, `${type} variable(${length}, ${which})`);
        assert.eq(exports.variableFirst(length), 0, `${type} variableFirst(${length})`);
    }
    for (let length of constantLengths)
        assert.eq(exports[`constant${length}`](), 0, `${type} constant${length}`);
}

async function test() {
    for (let testCase of cases)
        await testArray(testCase);
}

await assert.asyncTest(test())
