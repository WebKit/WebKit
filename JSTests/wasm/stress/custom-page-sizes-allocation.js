//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=0")

import * as assert from "../assert.js";

// A memory's byte length is its page count times its own page size, so a small-page memory is
// allocated, sized and grown in bytes. Its mapping is still made in whole OS pages, which the
// wasm-visible size must not expose.

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
function limits(initial, log2PageSize, maximum, shared) {
    const hasMax = maximum !== undefined;
    const flags = (hasMax ? 0x01 : 0x00) | (shared ? 0x02 : 0x00);
    if (log2PageSize === undefined)
        return [flags, ...varuint(initial), ...(hasMax ? varuint(maximum) : [])];
    return [flags | 0x08, ...varuint(initial), ...(hasMax ? varuint(maximum) : []), log2PageSize];
}

// One memory, exported along with "size", "grow", and a "load8" that traps out of bounds.
function module(lim) {
    const types = [0x03, 0x60, 0x00, 0x01, i32, 0x60, 0x01, i32, 0x01, i32, 0x60, 0x01, i32, 0x01, i32];
    const name = s => [s.length, ...[...s].map(c => c.charCodeAt(0))];
    const exports = [
        ...name("mem"), 0x02, 0x00,
        ...name("size"), 0x00, 0x00,
        ...name("grow"), 0x00, 0x01,
        ...name("load8"), 0x00, 0x02,
    ];
    const sizeBody = [0x00, 0x3f, 0x00, 0x0b];
    const growBody = [0x00, 0x20, 0x00, 0x40, 0x00, 0x0b];
    const loadBody = [0x00, 0x20, 0x00, 0x2d, 0x00, 0x00, 0x0b];
    const bodies = [sizeBody, growBody, loadBody];
    return new Uint8Array([
        ...header,
        ...section(0x01, types),
        ...section(0x03, [bodies.length, 0x00, 0x01, 0x02]),
        ...section(0x05, [0x01, ...lim]),
        ...section(0x07, [0x04, ...exports]),
        ...section(0x0a, [bodies.length, ...bodies.flatMap(b => [b.length, ...b])]),
    ]);
}

function instantiate(lim) {
    return new WebAssembly.Instance(new WebAssembly.Module(module(lim)));
}

// A one-page small-page memory is one byte long, and the byte past it traps.
{
    const { mem, size, load8 } = instantiate(limits(1, 0)).exports;
    assert.eq(size(), 1);
    assert.eq(mem.buffer.byteLength, 1);
    assert.eq(load8(0), 0);
    assert.throws(() => load8(1), WebAssembly.RuntimeError, "Out of bounds memory access");
}

// A zero-page memory instantiates and is empty, with or without a maximum.
for (const maximum of [undefined, 0, 16]) {
    const { mem, size, load8 } = instantiate(limits(0, 0, maximum)).exports;
    assert.eq(size(), 0);
    assert.eq(mem.buffer.byteLength, 0);
    assert.throws(() => load8(0), WebAssembly.RuntimeError, "Out of bounds memory access");
}

// Growing one byte at a time stays linear and keeps every byte written so far. Each step writes the
// newly exposed byte, so a reallocating grow that dropped or duplicated bytes would show up here.
{
    const iterations = 2000;
    const { mem, size, grow } = instantiate(limits(1, 0, iterations + 1)).exports;
    new Uint8Array(mem.buffer)[0] = 0xab;
    for (let n = 1; n <= iterations; ++n) {
        assert.eq(grow(1), n);
        assert.eq(size(), n + 1);
        const bytes = new Uint8Array(mem.buffer);
        assert.eq(bytes.length, n + 1);
        assert.eq(bytes[0], 0xab);
        assert.eq(bytes[n], 0);
        bytes[n] = 0xcd;
    }
    const bytes = new Uint8Array(mem.buffer);
    let written = 0;
    for (let i = 1; i <= iterations; ++i)
        written += bytes[i] === 0xcd;
    assert.eq(written, iterations);
}

// Data written before a grow survives it, including a grow that crosses an OS page boundary. The
// initial size is deliberately larger than any OS page this runs on.
{
    const initial = 20000;
    const delta = 20000;
    const { mem, grow } = instantiate(limits(initial, 0, initial + delta)).exports;
    const before = new Uint8Array(mem.buffer);
    for (let i = 0; i < initial; ++i)
        before[i] = (i * 7 + 3) & 0xff;

    assert.eq(grow(delta), initial);

    const after = new Uint8Array(mem.buffer);
    assert.eq(after.length, initial + delta);
    // Counting into one assertion per region, rather than asserting per byte, keeps this fast
    // enough to run in every tier.
    let preserved = 0;
    for (let i = 0; i < initial; ++i)
        preserved += after[i] === ((i * 7 + 3) & 0xff);
    assert.eq(preserved, initial);
    let grown = 0;
    for (let i = initial; i < initial + delta; ++i)
        grown += after[i];
    assert.eq(grown, 0);
}

// Newly grown bytes read as zero even when the grow stays inside an OS page that has been written
// to in full, which is the case where nothing is reallocated.
{
    const { mem, grow } = instantiate(limits(8, 0, 64)).exports;
    new Uint8Array(mem.buffer).fill(0xff);
    assert.eq(grow(24), 8);
    const bytes = new Uint8Array(mem.buffer);
    assert.eq(bytes.length, 32);
    for (let i = 0; i < 8; ++i)
        assert.eq(bytes[i], 0xff);
    for (let i = 8; i < 32; ++i)
        assert.eq(bytes[i], 0);
}

// An i32 memory may declare every byte of its address space with a one-byte page size. That is
// within what can be allocated, so it instantiates; the mapping is lazy, so it costs little.
{
    const { mem, size } = instantiate(limits(4294967295, 0)).exports;
    assert.eq(mem.buffer.byteLength, 4294967295);
    assert.eq(size(), -1); // 0xffffffff as an i32.
}

// A memory declared larger than can be allocated fails at instantiation, as an error rather than a
// crash. An i64 memory's declarable range reaches far past what any process can map.
{
    // Just the memory, since memory.size on an i64 memory does not return an i32.
    const memoryOnlyModule = lim => new Uint8Array([...header, ...section(0x05, [0x01, ...lim])]);
    const i64Limits = initial => [0x0c, ...varuint(initial), 0x00]; // i64 index type, explicit page size.
    assert.throws(() => new WebAssembly.Instance(new WebAssembly.Module(memoryOnlyModule(i64Limits(2 ** 40)))),
        RangeError, "Out of memory");
}

// A shared small-page memory is refused for now rather than allocated incorrectly. This pins the
// current behavior so that making it work is a visible change.
// FIXME: Shared memories grow through SharedArrayBufferContents, which works in OS pages
// throughout. Nothing in the proposal forbids this combination.
// https://bugs.webkit.org/show_bug.cgi?id=TODO
{
    assert.throws(() => instantiate(limits(1, 0, 16, true)), RangeError, "Out of memory");
}

// A 64 KiB-page memory allocates, sizes and grows exactly as it does without this feature, whether
// its page size is implicit or stated.
for (const log2PageSize of [16, undefined]) {
    const { mem, size, grow, load8 } = instantiate(limits(1, log2PageSize, 4)).exports;
    assert.eq(size(), 1);
    assert.eq(mem.buffer.byteLength, 65536);
    assert.throws(() => load8(65536), WebAssembly.RuntimeError, "Out of bounds memory access");

    new Uint8Array(mem.buffer).fill(0x5a);
    assert.eq(grow(2), 1);
    assert.eq(size(), 3);

    const bytes = new Uint8Array(mem.buffer);
    assert.eq(bytes.length, 3 * 65536);
    // Only the bytes either side of the old boundary can tell a correct grow from one that copied
    // or zeroed the wrong range, so walking the whole memory would just cost time.
    assert.eq(bytes[0], 0x5a);
    assert.eq(bytes[65535], 0x5a);
    assert.eq(bytes[65536], 0);
    assert.eq(bytes[3 * 65536 - 1], 0);

    assert.eq(grow(2), -1);
    assert.eq(size(), 3);
    assert.eq(grow(1), 3);
    assert.eq(grow(1), -1);
}
