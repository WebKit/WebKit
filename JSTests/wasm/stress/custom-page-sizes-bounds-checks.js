//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=0")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmFastMemory=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1", "--useWasmIPInt=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeAfterWarmUp=10")

import * as assert from "../assert.js";

// A memory is addressable exactly up to its byte length. For a memory whose pages are smaller
// than an OS page that length need not be where the mapping ends, so the check cannot be against
// the mapped capacity. Every case here is expressed against the memory's own byteLength, so it
// states the same invariant whatever the page size scales to.

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
const i64 = 0x7e;

// Each load/store opcode with the width it touches, so a test can say where the access must stop.
const accesses = [
    { name: "i32.load8_u", load: 0x2d, store: 0x3a, width: 1, type: i32 },
    { name: "i32.load16_u", load: 0x2f, store: 0x3b, width: 2, type: i32 },
    { name: "i32.load", load: 0x28, store: 0x36, width: 4, type: i32 },
    { name: "i64.load", load: 0x29, store: 0x37, width: 8, type: i64 },
];

// Exports "load" and "store", both taking an address, plus the memory itself. The memarg's
// static offset is folded into the bounds check differently from the dynamic address, so it is
// a parameter here.
function accessModule(limits, access, staticOffset) {
    const types = [
        0x02,
        0x60, 0x01, i32, 0x01, access.type,
        0x60, 0x01, i32, 0x00,
    ];
    const funcs = [0x02, 0x00, 0x01];
    const exports = [
        0x03,
        0x04, 0x6c, 0x6f, 0x61, 0x64, 0x00, 0x00,
        0x05, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x00, 0x01,
        0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00,
    ];
    const loadBody = [0x00, 0x20, 0x00, access.load, 0x00, ...varuint(staticOffset), 0x0b];
    const zero = access.type === i64 ? [0x42, 0x00] : [0x41, 0x00];
    const storeBody = [0x00, 0x20, 0x00, ...zero, access.store, 0x00, ...varuint(staticOffset), 0x0b];
    const code = [
        0x02,
        ...varuint(loadBody.length), ...loadBody,
        ...varuint(storeBody.length), ...storeBody,
    ];
    return new Uint8Array([
        ...header,
        ...section(0x01, types),
        ...section(0x03, funcs),
        ...section(0x05, [0x01, ...limits]),
        ...section(0x07, exports),
        ...section(0x0a, code),
    ]);
}

// Limits bytes. Omitting log2PageSize gives the pre-proposal encoding, meaning 64 KiB.
function limits(initial, log2PageSize, maximum) {
    const hasMax = maximum !== undefined;
    if (log2PageSize === undefined)
        return [hasMax ? 0x01 : 0x00, ...varuint(initial), ...(hasMax ? varuint(maximum) : [])];
    return [hasMax ? 0x09 : 0x08, ...varuint(initial), ...(hasMax ? varuint(maximum) : []), log2PageSize];
}

function traps(fn, address) {
    assert.throws(() => fn(address), WebAssembly.RuntimeError, "Out of bounds memory access");
}

// The last address an access of this width may start at, and the first it may not.
function checkBoundary(instance, access, staticOffset, byteLength) {
    const { load, store } = instance.exports;
    const lastValid = byteLength - access.width - staticOffset;

    if (lastValid >= 0) {
        load(lastValid);
        store(lastValid);
        if (lastValid > 0) {
            load(0);
            store(0);
        }
    } else {
        // The access cannot fit at any address, not even zero.
        traps(load, 0);
        traps(store, 0);
    }

    for (const address of [lastValid + 1, byteLength, byteLength + 1]) {
        if (address < 0)
            continue;
        traps(load, address);
        traps(store, address);
    }
}

function instantiate(limitsBytes, access, staticOffset) {
    return new WebAssembly.Instance(new WebAssembly.Module(accessModule(limitsBytes, access, staticOffset)));
}

// Every access width, at several static offsets, for both page sizes. A memory of three pages is
// small enough that a wide access near the end cannot fit, which is the case that distinguishes
// checking the access's end from checking its start.
for (const log2PageSize of [0, 16, undefined]) {
    for (const access of accesses) {
        for (const staticOffset of [0, 1, 7]) {
            const instance = instantiate(limits(3, log2PageSize), access, staticOffset);
            checkBoundary(instance, access, staticOffset, instance.exports.mem.buffer.byteLength);
        }
    }
}

// A large static offset is materialized rather than folded, and on the signaling path it is what
// makes an explicit check appear at all.
for (const log2PageSize of [0, 16]) {
    for (const access of accesses) {
        const instance = instantiate(limits(3, log2PageSize), access, 70000);
        checkBoundary(instance, access, 70000, instance.exports.mem.buffer.byteLength);
    }
}

// A zero-length memory admits no access at all.
for (const log2PageSize of [0, 16, undefined]) {
    for (const access of accesses) {
        const instance = instantiate(limits(0, log2PageSize), access, 0);
        assert.eq(instance.exports.mem.buffer.byteLength, 0);
        traps(instance.exports.load, 0);
        traps(instance.exports.store, 0);
    }
}

// Growing republishes the bounds value. This must hold even when the growth fits inside the
// already-mapped region, where the mapped capacity does not move.
for (const log2PageSize of [0, 16, undefined]) {
    for (const delta of [1, 2]) {
        const access = accesses[0];
        const instance = instantiate(limits(3, log2PageSize, 16), access, 0);
        const before = instance.exports.mem.buffer.byteLength;
        checkBoundary(instance, access, 0, before);

        assert.eq(instance.exports.mem.grow(delta), 3);
        const after = instance.exports.mem.buffer.byteLength;
        assert.truthy(after > before);
        checkBoundary(instance, access, 0, after);
    }
}

// The boundary must still hold once the function is hot, since tiering up and OSR entry each
// re-publish the bounds value by their own path.
for (const log2PageSize of [0, 16, undefined]) {
    const access = accesses[0];
    const instance = instantiate(limits(3, log2PageSize), access, 0);
    const byteLength = instance.exports.mem.buffer.byteLength;
    const { load } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; ++i)
        load(0);

    checkBoundary(instance, access, 0, byteLength);
}
