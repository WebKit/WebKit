//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const patterns = [];
for (const elementSize of [1, 2, 4, 8]) {
    for (const base of [0, 16]) {
        for (let lane = 0; lane < 16 / elementSize; ++lane) {
            const pattern = [];
            for (let i = 0; i < 16; ++i)
                pattern.push(base + lane * elementSize + i % elementSize);
            patterns.push(pattern);
        }
    }
}

async function test() {
    let source = "";
    patterns.forEach((pattern, index) => {
        source += `
      (func (export "dup${index}") (param i32 i32 i32)
        (v128.store (local.get 2)
          (i8x16.shuffle ${pattern.join(" ")} (v128.load (local.get 0)) (v128.load (local.get 1)))))`;
    });
    const instance = await instantiate(`(module (memory (export "mem") 1) ${source})`, {}, { simd: true });
    const memory = new Uint8Array(instance.exports.mem.buffer);
    const input = [];
    for (let i = 0; i < 32; ++i) {
        input.push((0x81 + i * 7) & 0xff);
        memory[i] = input[i];
    }
    const funcs = patterns.map((_, index) => instance.exports[`dup${index}`]);
    const expected = patterns.map(pattern => pattern.map(index => input[index]));

    for (let iteration = 0; iteration < wasmTestLoopCount; ++iteration) {
        for (let index = 0; index < funcs.length; ++index) {
            funcs[index](0, 16, 64);
            const bytes = expected[index];
            for (let i = 0; i < 16; ++i) {
                if (memory[64 + i] !== bytes[i])
                    throw new Error(`dup${index} byte ${i}: expected ${bytes[i]} but got ${memory[64 + i]}`);
            }
        }
    }
}

await assert.asyncTest(test());
