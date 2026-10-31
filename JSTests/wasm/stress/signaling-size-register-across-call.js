//@ requireOptions("--useWasmFastMemory=true", "--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const valueCount = 32;
const valueParams = new Array(valueCount).fill("i32").join(" ");
const sets = [];
const gets = [];
for (let i = 1; i <= valueCount; ++i) {
    sets.push(`(local.set ${i} (i32.const ${i}))`);
    gets.push(`(local.get ${i})`);
}

const watCaller = `
(module
    (type $sig (func (param i32 ${valueParams})))
    (import "e" "mem" (memory 1 1))
    (table (export "tbl") 1 1 funcref)
    (func (export "f") (param $oob i32) (local ${valueParams})
        ${sets.join("\n        ")}
        (return_call_indirect (type $sig)
            (select (i32.const 0x20000) (i32.const 0) (local.get $oob))
            ${gets.join("\n            ")}
            (i32.const 0))))
`;

const calleeLoads = [];
for (let i = 1; i <= valueCount; ++i)
    calleeLoads.push(`(local.get ${i})`);
const calleeAdds = Array.from({ length: valueCount - 1 }, () => "(i32.add)");

const watCallee = `
(module
    (import "e" "mem" (memory 1 1))
    (func (export "g") (param i32 ${valueParams})
        (local.get 0)
        ${calleeLoads.join("\n        ")}
        ${calleeAdds.join("\n        ")}
        (i32.store)))
`;

async function test() {
    const memCaller = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "Signaling");
    const memCallee = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "BoundsChecking");
    assert.eq(WebAssemblyMemoryMode(memCaller), "Signaling");
    assert.eq(WebAssemblyMemoryMode(memCallee), "BoundsChecking");

    const instCaller = await instantiate(watCaller, { e: { mem: memCaller } }, { tail_call: true });
    const instCallee = await instantiate(watCallee, { e: { mem: memCallee } });
    const { f, tbl } = instCaller.exports;
    const { g } = instCallee.exports;

    const inBounds = [0, ...new Array(valueCount).fill(1)];
    for (let i = 0; i < wasmTestLoopCount; ++i)
        g(...inBounds);

    tbl.set(0, g);

    // Caller locals are 1..valueCount. The callee stores their sum at address 0.
    const expectedSum = valueCount * (valueCount + 1) / 2;
    const calleeMemory = new Int32Array(memCallee.buffer);
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        f(0);
        assert.eq(calleeMemory[0], expectedSum);
    }
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.throws(() => f(1), WebAssembly.RuntimeError, "Out of bounds memory access");
}

await assert.asyncTest(test());
