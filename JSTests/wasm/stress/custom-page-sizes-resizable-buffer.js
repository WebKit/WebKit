//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")

import * as assert from "../assert.js";

// A wasm memory exposed through toResizableBuffer() grows by whole pages of its own size, which
// need not be the default one. Both the multiple-of check and the delta handed to Memory::grow
// are in that unit.

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

// Limits bytes. Omitting log2PageSize gives the pre-proposal encoding, meaning 64 KiB.
function limits(initial, log2PageSize, maximum) {
    const hasMax = maximum !== undefined;
    if (log2PageSize === undefined)
        return [hasMax ? 0x01 : 0x00, ...varuint(initial), ...(hasMax ? varuint(maximum) : [])];
    return [hasMax ? 0x09 : 0x08, ...varuint(initial), ...(hasMax ? varuint(maximum) : []), log2PageSize];
}

function exportedMemory(limitsBytes) {
    const bytes = new Uint8Array([
        ...header,
        ...section(0x05, [0x01, ...limitsBytes]),
        ...section(0x07, [0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00]),
    ]);
    return new WebAssembly.Instance(new WebAssembly.Module(bytes)).exports.mem;
}

// A one-byte page size: every byte length is a whole number of pages, so any resize is legal.
{
    const memory = exportedMemory(limits(3, 0, 16));
    const buffer = memory.toResizableBuffer();
    assert.eq(buffer.byteLength, 3);
    assert.truthy(buffer.resizable);

    buffer.resize(4);
    assert.eq(memory.buffer.byteLength, 4);

    // A one-byte growth is a whole page here, where it would be rejected for a 64 KiB memory.
    buffer.resize(5);
    assert.eq(memory.buffer.byteLength, 5);

    buffer.resize(16);
    assert.eq(memory.buffer.byteLength, 16);
}

// The default page size keeps rejecting anything that is not a multiple of it.
for (const log2PageSize of [16, undefined]) {
    const memory = exportedMemory(limits(1, log2PageSize, 4));
    const buffer = memory.toResizableBuffer();
    assert.eq(buffer.byteLength, 65536);

    assert.throws(() => buffer.resize(65537), RangeError, "is not a multiple of 65536");
    assert.eq(memory.buffer.byteLength, 65536);

    buffer.resize(131072);
    assert.eq(memory.buffer.byteLength, 131072);
}

// Shrinking is refused whatever the page size.
for (const log2PageSize of [0, 16, undefined]) {
    const memory = exportedMemory(limits(2, log2PageSize, 4));
    const buffer = memory.toResizableBuffer();
    const before = buffer.byteLength;
    assert.throws(() => buffer.resize(0), RangeError, "Cannot shrink WebAssembly memory");
    assert.eq(memory.buffer.byteLength, before);
}
