//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";

// A memidx is a u32 LEB128, so a non-minimal encoding of it is still valid. Only memory.size and
// memory.grow ever read one outside a memarg, and IPInt has to advance past however many bytes the
// encoding actually took.

function leb(value, byteCount) {
    const bytes = [];
    do {
        bytes.push(value & 0x7f);
        value >>>= 7;
    } while (value || bytes.length < byteCount);
    for (let i = 0; i < bytes.length - 1; ++i)
        bytes[i] |= 0x80;
    return bytes;
}

function build(memoryIndexBytes) {
    const body = [
        0x00,                       // no locals
        0x3f, ...memoryIndexBytes,  // memory.size <memidx>
        0x41, 0x01,                 // i32.const 1
        0x40, ...memoryIndexBytes,  // memory.grow <memidx>
        0x1a,                       // drop
        0x0b,
    ];
    const codeSection = [0x01, ...leb(body.length), ...body];
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,       // type 0: () -> i32
        0x03, 0x02, 0x01, 0x00,                         // func 0: type 0
        0x05, 0x07, 0x02, 0x01, 0x01, 0x04, 0x01, 0x01, 0x04, // memory 0 and memory 1, each 1..4 pages
        0x07, 0x08, 0x01, 0x04, 0x73, 0x69, 0x7a, 0x65, 0x00, 0x00, // export "size" -> func 0
        0x0a, ...leb(codeSection.length), ...codeSection,
    ]);
}

for (const byteCount of [1, 2, 3, 5]) {
    for (const memoryIndex of [0, 1]) {
        const instance = new WebAssembly.Instance(new WebAssembly.Module(build(leb(memoryIndex, byteCount)).buffer));
        assert.eq(instance.exports.size(), 1);
        assert.eq(instance.exports.size(), 2);
    }
}

// Six bytes is past what a u32 can encode.
assert.throws(() => new WebAssembly.Module(build(leb(0, 6)).buffer), WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 7: can't get memory index, in function at index 0");
