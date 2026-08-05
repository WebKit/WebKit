//@ skip if $addressBits <= 32
import { compile } from "../wabt-wrapper.js";

// A memory64 currently forces a single-memory module, because IPInt derives the address width of
// every access from memory 0. The restriction has to hold whichever order the memories appear in.

const options = { memory64: true, multi_memory: true };

async function assertRejected(wat) {
    try {
        await compile(wat, options);
    } catch (e) {
        if (e instanceof WebAssembly.CompileError && e.message.includes("if using memory64 then multiple memories are illegal for now"))
            return;
        throw new Error(`Wrong error for ${wat}: ${e}`);
    }
    throw new Error(`Expected a CompileError for ${wat}`);
}

await assertRejected(`(module (memory i64 1) (memory 1))`);
await assertRejected(`(module (memory 1) (memory i64 1))`);
await assertRejected(`(module (memory i64 1) (memory i64 1))`);
await assertRejected(`(module (memory i64 1) (memory 1) (memory 1))`);
await assertRejected(`(module (import "m" "a" (memory i64 1)) (import "m" "b" (memory 1)))`);
await assertRejected(`(module (import "m" "a" (memory 1)) (import "m" "b" (memory i64 1)))`);
await assertRejected(`(module (import "m" "a" (memory i64 1)) (memory 1))`);

// Multiple memory32s remain legal, and so does a lone memory64.
await compile(`(module (memory 1) (memory 1))`, options);
await compile(`(module (memory i64 1))`, options);
