//@ requireOptions("--useWasmFastMemory=true", "--useWasmTailCalls=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// Live i64 temps across a call to a function that return_call_indirects into
// another instance. The temps are not locals: a stack slot would hide a
// clobber of wasmBoundsCheckingSizeRegister.
const valueCount = 24;
const temps = [];
const folds = [];
for (let i = 1; i <= valueCount; ++i)
    temps.push(`(local.get $seed) (i64.const ${i}) (i64.add)`);
for (let i = 1; i < valueCount; ++i)
    folds.push("(i64.xor)");
let expectedBits = 0n;
for (let i = 1; i <= valueCount; ++i)
    expectedBits ^= BigInt(i);

const watCaller = `
(module
    (type $v (func (param i32)))
    (import "e" "mem" (memory 1 1))
    (table (export "tbl") 1 1 funcref)
    (func $mid (param i32)
        (return_call_indirect (type $v) (local.get 0) (i32.const 0)))
    (func (export "f") (param $addr i32) (param $seed i64) (result i64)
        ${temps.join("\n        ")}
        (call $mid (local.get $addr))
        ${folds.join("\n        ")}))
`;

const watCallee = `
(module
    (import "e" "mem" (memory 1 1))
    (func (export "g") (param i32)
        (i32.store (local.get 0) (i32.const 9))))
`;

async function test() {
    const memCaller = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "Signaling");
    const memCallee = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "BoundsChecking");
    assert.eq(WebAssemblyMemoryMode(memCaller), "Signaling");
    assert.eq(WebAssemblyMemoryMode(memCallee), "BoundsChecking");

    const instCallee = await instantiate(watCallee, { e: { mem: memCallee } });
    const instCaller = await instantiate(watCaller, { e: { mem: memCaller } }, { tail_call: true });
    instCaller.exports.tbl.set(0, instCallee.exports.g);

    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(instCaller.exports.f(0, 0n), expectedBits);
    assert.throws(() => instCaller.exports.f(0x20000, 0n), WebAssembly.RuntimeError, "Out of bounds memory access");
}

await assert.asyncTest(test());
