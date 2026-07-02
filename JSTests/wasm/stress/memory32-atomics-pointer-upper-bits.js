//@ skip if $architecture != "arm64" && $architecture != "x86_64"
//@ runDefaultWasm("-m")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// In IPInt, i32.wrap_i64 is a no-op: the 64-bit stack slot keeps the upper
// 32 bits of the original i64. memory.atomic.notify/wait32/wait64 must
// normalize the i32 pointer operand to its low 32 bits before the bounds
// check; otherwise an in-bounds address with dirty upper bits is wrongly
// rejected as out-of-bounds.

const wat = `
(module
    (memory (export "memory") 1 1 shared)
    (func (export "notify") (param $dirty i64) (result i32)
        (memory.atomic.notify (i32.wrap_i64 (local.get $dirty)) (i32.const 0))
    )
    (func (export "wait32") (param $dirty i64) (result i32)
        (memory.atomic.wait32 (i32.wrap_i64 (local.get $dirty)) (i32.const 1) (i64.const 0))
    )
    (func (export "wait64") (param $dirty i64) (result i32)
        (memory.atomic.wait64 (i32.wrap_i64 (local.get $dirty)) (i64.const 1) (i64.const 0))
    )
)
`;

const { exports } = await instantiate(wat, {}, { threads: true });

for (let i = 0; i < wasmTestLoopCount; i++) {
    // Address 16 is in bounds; the upper 32 bits must be ignored.
    assert.eq(exports.notify(0x1_0000_0010n), 0);
    assert.eq(exports.notify(0xffff_ffff_0000_0010n), 0);
    // Memory contains 0, expected value is 1, so wait returns "not-equal" (1).
    assert.eq(exports.wait32(0x1_0000_0010n), 1);
    assert.eq(exports.wait32(0xdead_beef_0000_0010n), 1);
    assert.eq(exports.wait64(0x1_0000_0010n), 1);
    assert.eq(exports.wait64(0xdead_beef_0000_0010n), 1);

    // Last in-bounds aligned addresses.
    assert.eq(exports.notify(0x1_0000_fffcn), 0);
    assert.eq(exports.wait32(0x1_0000_fffcn), 1);
    assert.eq(exports.wait64(0x1_0000_fff8n), 1);

    // Out-of-bounds after wrapping must still trap.
    assert.throws(() => exports.notify(0x1_0001_0000n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => exports.wait32(0x1_0001_0000n), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.throws(() => exports.wait64(0x1_0001_0000n), WebAssembly.RuntimeError, "Out of bounds memory access");
}
