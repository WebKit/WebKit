//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// memory.atomic.wait32/wait64 with a Memory64 pointer + immediate offset that
// overflows 64 bits. The effective address must be computed without wrapping;
// an overflowing sum is out of bounds and must trap.

const wat = `
(module
    (memory (export "memory") i64 1 1 shared)
    (func (export "wait32") (param $addr i64) (result i32)
        (memory.atomic.wait32 offset=8 (local.get $addr) (i32.const 0) (i64.const 0))
    )
    (func (export "wait64") (param $addr i64) (result i32)
        (memory.atomic.wait64 offset=8 (local.get $addr) (i64.const 0) (i64.const 0))
    )
)
`;

const { exports } = await instantiate(wat, {}, { threads: true, memory64: true });

for (let i = 0; i < wasmTestLoopCount; i++) {
    // 0xFFFF_FFFF_FFFF_FFF8 + 8 wraps to 0; must trap as out of bounds.
    assert.throws(() => exports.wait32(0xffff_ffff_ffff_fff8n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => exports.wait64(0xffff_ffff_ffff_fff8n), WebAssembly.RuntimeError, "Out of bounds memory access");

    // 0xFFFF_FFFF_FFFF_FFFC + 8 wraps to 4; must trap as out of bounds.
    assert.throws(() => exports.wait32(0xffff_ffff_ffff_fffcn), WebAssembly.RuntimeError, "Out of bounds memory access");

    // In bounds, equal to expected value, timeout 0 -> returns 2 (TimedOut).
    assert.eq(exports.wait32(0n), 2);
    assert.eq(exports.wait64(0n), 2);
}
