//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmMemory64=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmMemory64=1", "--useWasmIPInt=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmMemory64=1", "--useWasmIPInt=0")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmMemory64=1", "--useWasmIPInt=0", "--thresholdForBBQOptimizeAfterWarmUp=0")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmMemory64=1", "--useWasmIPInt=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeAfterWarmUp=10")

import * as assert from "../assert.js";

// memory.size counts the memory's own pages, and memory.grow takes and returns a count in them.
// Both are a shift by the page size, which is static per memidx.

const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];

function varuint(value) {
    value = BigInt(value);
    const bytes = [];
    do {
        let byte = Number(value & 0x7fn);
        value >>= 7n;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, bytes) {
    return [id, ...varuint(bytes.length), ...bytes];
}

const i32 = 0x7f;

// Limits bytes. Omitting log2PageSize gives the pre-proposal encoding, meaning 64 KiB.
function limits(initial, log2PageSize, maximum) {
    const hasMax = maximum !== undefined;
    if (log2PageSize === undefined)
        return [hasMax ? 0x01 : 0x00, ...varuint(initial), ...(hasMax ? varuint(maximum) : [])];
    return [hasMax ? 0x09 : 0x08, ...varuint(initial), ...(hasMax ? varuint(maximum) : []), log2PageSize];
}

// One memory per entry in memories, each exporting "sizeN" and "growN" against its own memidx.
function module(memories) {
    const types = [0x02, 0x60, 0x00, 0x01, i32, 0x60, 0x01, i32, 0x01, i32];
    const funcs = [];
    const exports = [];
    const code = [];
    for (let i = 0; i < memories.length; ++i) {
        funcs.push(0x00, 0x01);
        const name = suffix => [...varuint(suffix.length), ...suffix].flat();
        exports.push(
            ...name([...`size${i}`].map(c => c.charCodeAt(0))), 0x00, 2 * i,
            ...name([...`grow${i}`].map(c => c.charCodeAt(0))), 0x00, 2 * i + 1);
        const sizeBody = [0x00, 0x3f, i, 0x0b];
        const growBody = [0x00, 0x20, 0x00, 0x40, i, 0x0b];
        code.push([sizeBody, growBody]);
    }
    const memorySection = [memories.length, ...memories.flat()];
    const codeSection = [code.length * 2, ...code.flat().flatMap(body => [body.length, ...body])];
    return new Uint8Array([
        ...header,
        ...section(0x01, types),
        ...section(0x03, [memories.length * 2, ...funcs]),
        ...section(0x05, memorySection),
        ...section(0x07, [memories.length * 2, ...exports]),
        ...section(0x0a, codeSection),
    ]);
}

function instantiate(memories) {
    return new WebAssembly.Instance(new WebAssembly.Module(module(memories)));
}

// The spec test's exact numbers for a one-byte page size.
{
    const { size0, grow0 } = instantiate([limits(0, 0, 131072)]).exports;
    assert.eq(size0(), 0);
    assert.eq(grow0(65536), 0);
    assert.eq(size0(), 65536);
    assert.eq(grow0(65536), 65536);
    assert.eq(size0(), 131072);
}

// Stating the default page size explicitly does not raise the cap.
{
    const { size0, grow0 } = instantiate([limits(0, 16, 16)]).exports;
    assert.eq(grow0(65537), -1);
    assert.eq(size0(), 0);
}

// A count of small pages is the count itself, not a byte count scaled by anything.
for (const log2PageSize of [0, 16, undefined]) {
    const { size0 } = instantiate([limits(3, log2PageSize)]).exports;
    assert.eq(size0(), 3);
}

// Growing by zero reports the current size and does not fail.
for (const log2PageSize of [0, 16, undefined]) {
    const { size0, grow0 } = instantiate([limits(3, log2PageSize)]).exports;
    assert.eq(grow0(0), 3);
    assert.eq(size0(), 3);
}

// Growing past a declared maximum fails and leaves the size alone.
for (const log2PageSize of [0, 16, undefined]) {
    const { size0, grow0 } = instantiate([limits(3, log2PageSize, 4)]).exports;
    assert.eq(grow0(2), -1);
    assert.eq(size0(), 3);
    assert.eq(grow0(1), 3);
    assert.eq(size0(), 4);
}

// Multi-memory: each memidx uses its own page size, and memory 0 takes a different code path in
// every tier, so the small-page memory goes in each position in turn.
if (jscOptions().useWasmMultiMemory) {
    {
        const { size0, size1 } = instantiate([limits(3, 0), limits(3, 16)]).exports;
        assert.eq(size0(), 3);
        assert.eq(size1(), 3);
    }
    {
        const { size0, size1 } = instantiate([limits(3, 16), limits(3, 0)]).exports;
        assert.eq(size0(), 3);
        assert.eq(size1(), 3);
    }
    // Same byte length, different page sizes: only a per-memidx shift tells these apart.
    {
        const { size0, size1 } = instantiate([limits(65536, 0), limits(1, 16)]).exports;
        assert.eq(size0(), 65536);
        assert.eq(size1(), 1);
    }
}

// An i64 memory's page count may exceed 32 bits, which only a page size below the default makes
// reachable: 2**32 default-sized pages would be 2**48 bytes. The result must not be canonicalized
// to 32 bits the way an i32 one is.
{
    const i64 = 0x7e;
    const pageCount = 4294967296n + 7n;
    const bytes = new Uint8Array([
        ...header,
        ...section(0x01, [0x01, 0x60, 0x00, 0x01, i64]),
        ...section(0x03, [0x01, 0x00]),
        // flags 0x0c = is64 | hasPageSize
        ...section(0x05, [0x01, 0x0c, ...varuint(pageCount), 0x00]),
        ...section(0x07, [0x01, 0x04, 0x73, 0x69, 0x7a, 0x65, 0x00, 0x00]),
        ...section(0x0a, [0x01, 0x04, 0x00, 0x3f, 0x00, 0x0b]),
    ]);
    const { size } = new WebAssembly.Instance(new WebAssembly.Module(bytes)).exports;
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(size(), pageCount);
}

// The same count in an i32 memory, which is the case the canonicalization above exists for: the
// result must be exactly 2**32 - 1 with nothing in the upper half, so extending it unsigned gives
// that and not a sign-extended value.
{
    const i64 = 0x7e;
    const pageCount = 4294967295n;
    const bytes = new Uint8Array([
        ...header,
        ...section(0x01, [0x01, 0x60, 0x00, 0x01, i64]),
        ...section(0x03, [0x01, 0x00]),
        ...section(0x05, [0x01, 0x08, ...varuint(pageCount), 0x00]),
        ...section(0x07, [0x01, 0x04, 0x73, 0x69, 0x7a, 0x65, 0x00, 0x00]),
        // memory.size; i64.extend_i32_u
        ...section(0x0a, [0x01, 0x05, 0x00, 0x3f, 0x00, 0xad, 0x0b]),
    ]);
    const { size } = new WebAssembly.Instance(new WebAssembly.Module(bytes)).exports;
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(size(), pageCount);
}
