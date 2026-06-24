//@ runDefaultWasm("-m")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// In IPInt, i32.wrap_i64 is a no-op: the 64-bit stack slot keeps the upper
// 32 bits of the original i64. memory.grow must normalize the i32 delta to
// its low 32 bits; otherwise a valid grow request with dirty upper bits is
// wrongly rejected with -1.

const wat = `
(module
    (memory (export "memory") 1 8)
    (func (export "grow") (param $dirty i64) (result i32)
        (memory.grow (i32.wrap_i64 (local.get $dirty)))
    )
    (func (export "size") (result i32)
        (memory.size)
    )
)
`;

const { exports } = await instantiate(wat, {});

// Delta 1 with dirty upper bits: must grow by exactly 1 page.
assert.eq(exports.grow(0x1_0000_0001n), 1);
assert.eq(exports.size(), 2);
assert.eq(exports.grow(0xdead_beef_0000_0001n), 2);
assert.eq(exports.size(), 3);

for (let i = 0; i < wasmTestLoopCount; i++) {
    // Delta 0 with dirty upper bits: no-op grow returning the current size.
    assert.eq(exports.grow(0x1_0000_0000n), 3);
    assert.eq(exports.grow(0xffff_ffff_0000_0000n), 3);
    assert.eq(exports.size(), 3);

    // A genuinely huge u32 delta must still fail.
    assert.eq(exports.grow(0xffff_ffffn), -1);
}
