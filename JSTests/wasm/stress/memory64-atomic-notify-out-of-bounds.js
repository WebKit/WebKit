//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// memory.atomic.notify with a Memory64 pointer just past the 32-bit boundary.
// The memory is 1 page (64 KiB), so pointer 2^32 is out of bounds and must trap.
// This catches a pointer-truncation bug: 2^32 truncated to 32 bits is 0, which
// would (wrongly) be in bounds.

const wat = `
(module
    (memory (export "memory") i64 1)
    (func (export "notify") (param $addr i64) (result i32)
        (memory.atomic.notify (local.get $addr) (i32.const 0))
    )
)
`;

const { exports } = await instantiate(wat, {}, { threads: true, memory64: true });

for (let i = 0; i < wasmTestLoopCount; i++) {
    assert.throws(() => exports.notify(0x1_0000_0000n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => exports.notify(0xffff_ffff_ffff_ffffn), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(exports.notify(0n), 0);
}
