//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// array.fill and memory.fill are inlined as a few overlapping stores for short lengths and a store
// loop for longer ones, and call out for the longest. Sweep lengths across all of those for every
// element type and check that nothing outside the filled range changes.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const byteLength = 640;

const cases = [
    { type: "i8", valueType: "i32", size: 1, get: "array.get_u", eq: "i32.eq", sentinel: "(i32.const 0xc3)", first: "(i32.const 0x5b)", second: "(i32.const 0x7f)" },
    { type: "i16", valueType: "i32", size: 2, get: "array.get_u", eq: "i32.eq", sentinel: "(i32.const 0xc3c3)", first: "(i32.const 0x1234)", second: "(i32.const 0x7777)" },
    { type: "i32", valueType: "i32", size: 4, get: "array.get", eq: "i32.eq", sentinel: "(i32.const 0xc3c3c3c3)", first: "(i32.const 0x12345678)", second: "(i32.const 0x5a5a5a5a)" },
    { type: "f32", valueType: "f32", size: 4, get: "array.get", eq: "f32.eq", sentinel: "(f32.reinterpret_i32 (i32.const 0xc3c3c3c3))", first: "(f32.const 1.5)", second: "(f32.reinterpret_i32 (i32.const 0x41414141))" },
    { type: "i64", valueType: "i64", size: 8, get: "array.get", eq: "i64.eq", sentinel: "(i64.const 0xc3c3c3c3c3c3c3c3)", first: "(i64.const 0x123456789abcdef0)", second: "(i64.const 0x5a5a5a5a5a5a5a5a)" },
    { type: "f64", valueType: "f64", size: 8, get: "array.get", eq: "f64.eq", sentinel: "(f64.reinterpret_i64 (i64.const 0xc3c3c3c3c3c3c3c3))", first: "(f64.const 1.5)", second: "(f64.reinterpret_i64 (i64.const 0x4141414141414141))" },
    { type: "(ref null $s)", valueType: "(ref null $s)", size: 8, get: "array.get", eq: "ref.eq", sentinel: "(ref.null $s)", first: "(global.get $first)", second: "(global.get $second)" },
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

async function testArray({ type, valueType, size, get, eq, sentinel, first, second }) {
    const length = byteLength / size;
    const { test } = (await instantiate(`
        (module
            (type $s (struct (field i32)))
            (type $arr (array (mut ${type})))
            (global $first (ref null $s) (struct.new $s (i32.const 1)))
            (global $second (ref null $s) (struct.new $s (i32.const 2)))
            (global $a (mut (ref null $arr)) (ref.null $arr))

            (func $value (param $which i32) (result ${valueType})
                (if (result ${valueType}) (local.get $which) (then ${first}) (else ${second})))

            (func $reset
                (global.set $a (array.new $arr ${sentinel} (i32.const ${length}))))
            (func $fill (param $offset i32) (param $length i32) (param $which i32)
                (array.fill $arr (global.get $a) (local.get $offset) (call $value (local.get $which)) (local.get $length)))
            (func $fillFirst (param $offset i32) (param $length i32)
                (array.fill $arr (global.get $a) (local.get $offset) ${first} (local.get $length)))
            (func $fillSecond (param $offset i32) (param $length i32)
                (array.fill $arr (global.get $a) (local.get $offset) ${second} (local.get $length)))

            (func $check (param $offset i32) (param $length i32) (param $which i32) (result i32)
                (local $i i32)
                (local $end i32)
                (local $expected ${valueType})
                (local.set $end (i32.add (local.get $offset) (i32.add (local.get $length) (i32.const 16))))
                (if (i32.gt_u (local.get $end) (i32.const ${length})) (then (local.set $end (i32.const ${length}))))
                (loop $loop
                    (local.set $expected ${sentinel})
                    (if (i32.and (i32.ge_u (local.get $i) (local.get $offset))
                                 (i32.lt_u (local.get $i) (i32.add (local.get $offset) (local.get $length))))
                        (then (local.set $expected (call $value (local.get $which)))))
                    (if (i32.eqz (${eq} (${get} $arr (global.get $a) (local.get $i)) (local.get $expected)))
                        (then (return (i32.add (local.get $i) (i32.const 1)))))
                    (local.set $i (i32.add (local.get $i) (i32.const 1)))
                    (br_if $loop (i32.lt_u (local.get $i) (local.get $end))))
                (i32.const 0))

            (func (export "test") (param $offset i32) (param $length i32) (param $mode i32) (result i32)
                (call $reset)
                (block $done
                    (block $second
                        (block $first
                            (block $variable
                                (br_table $variable $variable $first $second (local.get $mode)))
                            (call $fill (local.get $offset) (local.get $length) (local.get $mode))
                            (br $done))
                        (call $fillFirst (local.get $offset) (local.get $length))
                        (return (call $check (local.get $offset) (local.get $length) (i32.const 1))))
                    (call $fillSecond (local.get $offset) (local.get $length))
                    (return (call $check (local.get $offset) (local.get $length) (i32.const 0))))
                (call $check (local.get $offset) (local.get $length) (local.get $mode))))
    `)).exports;

    // Modes 0 and 1 fill with a runtime pattern, and modes 2 and 3 with a constant one.
    for (let offset of [0, 1]) {
        for (let count of lengths(length - offset)) {
            for (let mode = 0; mode < 4; ++mode)
                assert.eq(test(offset, count, mode), 0, `${type} offset ${offset} length ${count} mode ${mode}`);
        }
    }
}

async function testMemory() {
    const { memory, fill, fillZero } = (await instantiate(`
        (module
            (memory (export "memory") 1)
            (func (export "fill") (param $offset i32) (param $value i32) (param $length i32)
                (memory.fill (local.get $offset) (local.get $value) (local.get $length)))
            (func (export "fillZero") (param $offset i32) (param $length i32)
                (memory.fill (local.get $offset) (i32.const 0) (local.get $length))))
    `)).exports;

    const bytes = new Uint8Array(memory.buffer);
    const end = byteLength + 64;
    for (let offset of [0, 3]) {
        for (let count of lengths(byteLength)) {
            for (let value of [0, 0x1ff]) {
                bytes.fill(0xc3, 0, end);
                if (value)
                    fill(offset, value, count);
                else
                    fillZero(offset, count);
                for (let i = 0; i < end; ++i) {
                    let expected = i >= offset && i < offset + count ? value & 0xff : 0xc3;
                    if (bytes[i] !== expected)
                        throw new Error(`memory.fill(${offset}, ${value}, ${count}): byte ${i} is ${bytes[i]}`);
                }
            }
        }
    }
}

async function test() {
    for (let testCase of cases)
        await testArray(testCase);
    await testMemory();
}

await assert.asyncTest(test())
