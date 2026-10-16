//@ requireOptions("--useWasmFastMemory=true", "--wasmInliningMaximumWasmCalleeSize=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// More live i64 temps than the other allocatable GPRs, so one of them has to
// sit in wasmBoundsCheckingSizeRegister. These are not locals: a local is
// stored to its slot, and a register clobber would still read the old slot.
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
    (import "e" "g" (func $g (param i32)))
    (table (export "tbl") 1 1 funcref)
    (func $inner (param i32)
        (i32.store (local.get 0) (i32.const 7)))
    (func (export "direct") (param $addr i32) (param $seed i64) (result i64)
        ${temps.join("\n        ")}
        (call $inner (local.get $addr))
        ${folds.join("\n        ")})
    (func (export "imported") (param $addr i32) (param $seed i64) (result i64)
        ${temps.join("\n        ")}
        (call $g (local.get $addr))
        ${folds.join("\n        ")})
    (func (export "indirect") (param $addr i32) (param $seed i64) (result i64)
        ${temps.join("\n        ")}
        (call_indirect (type $v) (local.get $addr) (i32.const 0))
        ${folds.join("\n        ")}))
`;

const watCallee = `
(module
    (import "e" "mem" (memory 1 1))
    (func (export "g") (param i32)
        (i32.store (local.get 0) (i32.const 9))))
`;

const watGrow = `
(module
    (import "e" "mem" (memory 1 2))
    (func $grow (result i32)
        (memory.grow (i32.const 1)))
    (func (export "f") (param $doGrow i32) (result i32)
        (local $old i32)
        (if (local.get $doGrow)
            (then
                (local.set $old (call $grow))
                (i32.store (i32.mul (local.get $old) (i32.const 65536)) (i32.const 42))
                (return (i32.load (i32.mul (local.get $old) (i32.const 65536))))))
        (i32.const 0)))
`;

async function testLiveAcrossCall() {
    const memCaller = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "Signaling");
    const memCallee = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "BoundsChecking");
    assert.eq(WebAssemblyMemoryMode(memCaller), "Signaling");
    assert.eq(WebAssemblyMemoryMode(memCallee), "BoundsChecking");

    const instCallee = await instantiate(watCallee, { e: { mem: memCallee } });
    const instCaller = await instantiate(watCaller, { e: { mem: memCaller, g: instCallee.exports.g } });
    const { direct, imported, indirect, tbl } = instCaller.exports;
    tbl.set(0, instCallee.exports.g);

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(direct(0, 0n), expectedBits);
        assert.eq(imported(0, 0n), expectedBits);
        assert.eq(indirect(0, 0n), expectedBits);
    }
    assert.throws(() => direct(0x20000, 0n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => imported(0x20000, 0n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => indirect(0x20000, 0n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(direct(0, 0n), expectedBits);
}

async function testGrowUpdatesBoundsCheck() {
    const mem = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 2 }, "BoundsChecking");
    assert.eq(WebAssemblyMemoryMode(mem), "BoundsChecking");
    const inst = await instantiate(watGrow, { e: { mem } });
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(inst.exports.f(0), 0);
    assert.eq(inst.exports.f(1), 42);
}

const watCatchGrow = `
(module
    (import "e" "mem" (memory 1 2))
    (tag $t)
    (func $callee
        (drop (memory.grow (i32.const 1)))
        (throw $t))
    (func (export "f") (param $doGrow i32) (result i32)
        (local $old i32)
        (local.set $old (memory.size))
        (if (local.get $doGrow)
            (then
                (try
                    (do
                        (call $callee))
                    (catch $t
                        (i32.store
                            (i32.mul (local.get $old) (i32.const 65536))
                            (i32.const 42))
                        (return (i32.load
                            (i32.mul (local.get $old) (i32.const 65536))))))))
        (i32.const 0)))
`;

async function testCatchSeesCalleeGrow() {
    const mem = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 2 }, "BoundsChecking");
    assert.eq(WebAssemblyMemoryMode(mem), "BoundsChecking");
    const inst = await instantiate(watCatchGrow, { e: { mem } }, { exceptions: true });
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(inst.exports.f(0), 0);
    assert.eq(inst.exports.f(1), 42);
}

await assert.asyncTest(testLiveAcrossCall());
await assert.asyncTest(testGrowUpdatesBoundsCheck());
await assert.asyncTest(testCatchSeesCalleeGrow());
