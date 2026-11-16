//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Shuffles that read only one of their two inputs, including duplicated lanes of every width.
const patterns = [
    [0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3],
    [12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15],
    [20, 21, 22, 23, 20, 21, 22, 23, 20, 21, 22, 23, 20, 21, 22, 23],
    [5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5],
    [31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31],
    [6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7],
    [18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19],
    [8, 9, 10, 11, 12, 13, 14, 15, 8, 9, 10, 11, 12, 13, 14, 15],
    [16, 17, 18, 19, 20, 21, 22, 23, 16, 17, 18, 19, 20, 21, 22, 23],
    [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0],
    [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3],
    [31, 16, 30, 17, 29, 18, 28, 19, 27, 20, 26, 21, 25, 22, 24, 23],
];

async function test() {
    let funcs = "";
    patterns.forEach((pattern, index) => {
        funcs += `
      (func (export "loaded${index}") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle ${pattern.join(" ")} (v128.load (local.get 0)) (v128.load (local.get 1)))))
      (func (export "constant${index}") (param i32 i32)
        (v128.store (local.get 1)
          (i8x16.shuffle ${pattern.join(" ")} (v128.load (local.get 0)) (v128.const i32x4 0x13121110 0x17161514 0x1b1a1918 0x1f1e1d1c))))`;
    });
    const instance = await instantiate(`(module (memory (export "mem") 1) ${funcs})`, {}, { simd: true });
    const memory = new Uint8Array(instance.exports.mem.buffer);
    for (let i = 0; i < 32; ++i)
        memory[i] = i;

    for (let iteration = 0; iteration < wasmTestLoopCount; ++iteration) {
        patterns.forEach((pattern, index) => {
            instance.exports[`loaded${index}`](0, 16, 64);
            instance.exports[`constant${index}`](0, 96);
            for (let i = 0; i < 16; ++i) {
                assert.eq(memory[64 + i], pattern[i]);
                assert.eq(memory[96 + i], pattern[i]);
            }
        });
    }
}

await assert.asyncTest(test());
