//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const output = 2048;
const vectorCount = 48;
const maxIterations = 32;

function dot(a, b) {
    return `(i32x4.dot_i16x8_s (v128.load offset=${a * 16} (local.get $p)) (v128.load offset=${b * 16} (local.get $p)))`;
}

function chainOfDots(accumulator, pairs) {
    let tree = accumulator;
    for (let [a, b] of pairs)
        tree = `(i32x4.add ${tree} ${dot(a, b)})`;
    return tree;
}

const manyPairs = [[1, 2], [3, 4], [5, 6], [7, 8], [9, 1], [2, 3], [4, 5], [6, 7], [8, 9], [1, 3]];

let wat = `(module
  (memory (export "memory") 1)

  (func (export "loop") (param $n i32)
    (local $acc v128) (local $i i32) (local $p i32)
    (local.set $acc (v128.load (i32.const 0)))
    (loop $l
      (local.set $p (i32.shl (local.get $i) (i32.const 4)))
      (local.set $acc ${chainOfDots("(local.get $acc)", [[1, 2], [3, 4], [5, 6], [7, 8]])})
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (v128.store (i32.const ${output}) (local.get $acc)))

  (func (export "mixed")
    (local $p i32)
    (v128.store (i32.const ${output})
      (i32x4.add
        ${chainOfDots("(v128.load (i32.const 0))", [[1, 2], [3, 4]])}
        (i32x4.add
          (i32x4.extmul_low_i16x8_s (v128.load offset=80 (local.get $p)) (v128.load offset=96 (local.get $p)))
          (i32x4.extmul_high_i16x8_u (v128.load offset=80 (local.get $p)) (v128.load offset=96 (local.get $p)))))))

  (func (export "balanced")
    (local $p i32)
    (v128.store (i32.const ${output})
      (i32x4.add
        (i32x4.add ${dot(1, 2)} ${dot(3, 4)})
        (i32x4.add
          (i32x4.add (v128.load offset=80 (local.get $p)) (v128.load offset=96 (local.get $p)))
          (i32x4.add (v128.load offset=112 (local.get $p)) (v128.load offset=128 (local.get $p)))))))

  (func (export "many")
    (local $p i32)
    (v128.store (i32.const ${output}) ${chainOfDots("(v128.load (i32.const 0))", manyPairs)}))
)`;

function dotReference(i16, aIndex, bIndex) {
    let result = [];
    for (let lane = 0; lane < 4; ++lane) {
        let a = aIndex * 8 + lane * 2;
        let b = bIndex * 8 + lane * 2;
        result.push((i16[a] * i16[b] + i16[a + 1] * i16[b + 1]) | 0);
    }
    return result;
}

function addReference(a, b) {
    return a.map((value, lane) => (value + b[lane]) | 0);
}

function vectorAt(i32, index) {
    return Array.from(i32.subarray(index * 4, index * 4 + 4));
}

function checkOutput(i32, expected) {
    for (let lane = 0; lane < 4; ++lane)
        assert.eq(i32[output / 4 + lane], expected[lane]);
}

function makePattern(kind) {
    let pattern = new Int16Array(vectorCount * 8);
    let seed = kind + 1;
    for (let i = 0; i < pattern.length; ++i) {
        seed = (seed * 1103515245 + 12345) >>> 0;
        if (kind === 0)
            pattern[i] = -32768;
        else if (kind === 1 && i % 5 === 0)
            pattern[i] = i % 2 ? 32767 : -32768;
        else
            pattern[i] = seed >>> 16;
    }
    return pattern;
}

function computeExpected(pattern) {
    const i16 = pattern;
    const u16 = new Uint16Array(pattern.buffer);
    const i32 = new Int32Array(pattern.buffer);

    let loop = [];
    let accumulator = vectorAt(i32, 0);
    for (let i = 0; i < maxIterations; ++i) {
        for (let [a, b] of [[1, 2], [3, 4], [5, 6], [7, 8]])
            accumulator = addReference(accumulator, dotReference(i16, i + a, i + b));
        loop.push(accumulator);
    }

    let sum = addReference(addReference(vectorAt(i32, 0), dotReference(i16, 1, 2)), dotReference(i16, 3, 4));
    let low = [], high = [];
    for (let lane = 0; lane < 4; ++lane) {
        low.push(Math.imul(i16[5 * 8 + lane], i16[6 * 8 + lane]));
        high.push((u16[5 * 8 + 4 + lane] * u16[6 * 8 + 4 + lane]) | 0);
    }
    let mixed = addReference(sum, addReference(low, high));

    let dots = addReference(dotReference(i16, 1, 2), dotReference(i16, 3, 4));
    let others = addReference(addReference(vectorAt(i32, 5), vectorAt(i32, 6)), addReference(vectorAt(i32, 7), vectorAt(i32, 8)));
    let balanced = addReference(dots, others);

    let many = vectorAt(i32, 0);
    for (let [a, b] of manyPairs)
        many = addReference(many, dotReference(i16, a, b));

    return { loop, mixed, balanced, many };
}

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const { memory, loop, mixed, balanced, many } = instance.exports;
    const i16 = new Int16Array(memory.buffer);
    const i32 = new Int32Array(memory.buffer);

    const patterns = [0, 1, 2].map(makePattern);
    const expected = patterns.map(computeExpected);

    for (let round = 0; round < wasmTestLoopCount; ++round) {
        let kind = round % patterns.length;
        i16.set(patterns[kind]);

        let n = round % maxIterations + 1;
        loop(n);
        checkOutput(i32, expected[kind].loop[n - 1]);
        mixed();
        checkOutput(i32, expected[kind].mixed);
        balanced();
        checkOutput(i32, expected[kind].balanced);
        many();
        checkOutput(i32, expected[kind].many);
    }
}

await assert.asyncTest(test());
