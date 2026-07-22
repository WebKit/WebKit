//@ requireOptions("--useWasmFastMemory=true")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const watA = `
(module
    (type $sig (func (param i32 i64)))
    (import "e" "mem" (memory 1 1))
    (import "e" "nop" (func $nop))
    (table (export "tbl") 1 1 funcref)
    (func (export "f") (param $do_call i32) (param $off i32) (param $val i64)
        (call $nop)
        (if (local.get $do_call)
            (then (return_call_indirect (type $sig)
                    (local.get $off) (local.get $val) (i32.const 0))))))
`;

const watB = `
(module
    (import "e" "mem" (memory 1 1))
    (func (export "g") (param $off i32) (param $val i64)
        (i64.store (local.get $off) (local.get $val))))
`;

async function test() {
    const memA = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "Signaling");
    const memB = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "BoundsChecking");
    assert.eq(WebAssemblyMemoryMode(memA), "Signaling");
    assert.eq(WebAssemblyMemoryMode(memB), "BoundsChecking");

    const instA = await instantiate(watA, { e: { mem: memA, nop: () => {} } }, { tail_call: true });
    const { f, tbl } = instA.exports;

    const instB = await instantiate(watB, { e: { mem: memB } });
    const { g } = instB.exports;

    assert.throws(() => g(0x20000, 0n), WebAssembly.RuntimeError, "Out of bounds memory access");

    // Tier g into BBQ. IPInt's prologue reloads regCS4 on entry but BBQ does not.
    for (let i = 0; i < wasmTestLoopCount; ++i) g(0, 0n);

    // Tier f to OMG without ever taking the tail-call branch.
    for (let i = 0; i < wasmTestLoopCount; ++i) f(0, 0, 0n);

    tbl.set(0, g);

    assert.throws(() => f(1, 0x20000, 0n), WebAssembly.RuntimeError, "Out of bounds memory access");
}

await assert.asyncTest(test());
