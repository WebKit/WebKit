//@ skip if $addressBits <= 32
import { compile } from "../wabt-wrapper.js";

// Memory immediates have to be decoded the same way in unreachable code as in reachable code: a
// memory64 offset is a u64, and an atomic's align byte can carry a memory index. Getting this wrong
// rejects valid modules.

const options = { memory64: true, multi_memory: true, threads: true };

// A memory64 atomic offset above 2^32, in unreachable code.
await compile(`
(module
    (memory i64 1 1 shared)
    (func
        unreachable
        (drop (i32.atomic.load offset=0x100000000 (i64.const 0)))))
`, options);

// The same offset in reachable code has always worked; keep them side by side.
await compile(`
(module
    (memory i64 1 1 shared)
    (func (result i32)
        (i32.atomic.load offset=0x100000000 (i64.const 0))))
`, options);

// An atomic naming a non-default memory, in unreachable code.
await compile(`
(module
    (memory 1 1 shared)
    (memory $m 1 1 shared)
    (func
        unreachable
        (drop (i32.atomic.load $m (i32.const 0)))))
`, options);

// memory.size and memory.grow naming a non-default memory, in unreachable code.
await compile(`
(module
    (memory 1)
    (memory $m 1)
    (func
        unreachable
        (drop (memory.size $m))
        (drop (memory.grow $m (i32.const 1)))))
`, options);

// A memory64 offset above 2^32 on a plain load in unreachable code.
await compile(`
(module
    (memory i64 1)
    (func
        unreachable
        (drop (i32.load offset=0x100000000 (i64.const 0)))))
`, options);

// An out-of-range memory index in unreachable code is still an error.
async function assertRejected(wat, needle) {
    try {
        await compile(wat, options);
    } catch (e) {
        if (e instanceof WebAssembly.CompileError && e.message.includes(needle))
            return;
        throw new Error(`Wrong error: ${e}`);
    }
    throw new Error(`Expected a CompileError for ${wat}`);
}

await assertRejected(`
(module
    (memory 1)
    (func
        unreachable
        (drop (memory.size 3))))
`, "memory index 3 is out of range");
