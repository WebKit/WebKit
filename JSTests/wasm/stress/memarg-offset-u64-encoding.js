import * as assert from "../assert.js";

// memarg's offset field is a u64 for every memory; only its value is restricted by the memory's
// address type. An in-range offset written with a wider-than-minimal LEB is still well formed.

const SECTION_TYPE = 1;
const SECTION_FUNCTION = 3;
const SECTION_MEMORY = 5;
const SECTION_CODE = 10;

function leb(value) {
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

// The value's minimal encoding, padded out to byteCount bytes with redundant continuation bytes.
function paddedLeb(value, byteCount) {
    let bytes = [];
    for (let i = 0; i < byteCount; ++i) {
        bytes.push(Number(value & 0x7fn) | (i + 1 < byteCount ? 0x80 : 0));
        value >>= 7n;
    }
    if (value)
        throw new Error(`${value} does not fit in ${byteCount} LEB bytes`);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

function moduleBytes(isMemory64, body) {
    let bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    bytes.push(...section(SECTION_TYPE, [0x01, 0x60, 0x00, 0x00]));
    bytes.push(...section(SECTION_FUNCTION, [0x01, 0x00]));
    bytes.push(...section(SECTION_MEMORY, [0x01, isMemory64 ? 0x04 : 0x00, 0x01]));
    const code = [0x00, ...body, 0x0b];
    bytes.push(...section(SECTION_CODE, [0x01, ...leb(code.length), ...code]));
    return new Uint8Array(bytes);
}

// i32.const 0 / i32.load align=4 offset=<offset> / drop
const load32 = (offsetBytes) => [0x41, 0x00, 0x28, 0x02, ...offsetBytes, 0x1a];
// i64.const 0 / i32.load align=4 offset=<offset> / drop
const load64 = (offsetBytes) => [0x42, 0x00, 0x28, 0x02, ...offsetBytes, 0x1a];

function assertValid(description, isMemory64, body) {
    try {
        new WebAssembly.Module(moduleBytes(isMemory64, body));
    } catch (error) {
        throw new Error(`${description}: expected to compile, got ${error}`);
    }
}

function assertInvalid(description, isMemory64, body) {
    try {
        new WebAssembly.Module(moduleBytes(isMemory64, body));
    } catch (error) {
        assert.truthy(error instanceof WebAssembly.CompileError, `${description}: expected CompileError, got ${error}`);
        return;
    }
    throw new Error(`${description}: module was accepted but is invalid`);
}

// An in-range offset is accepted at every encoded width, up to u64's maximum of ten bytes.
for (let byteCount = 1; byteCount <= 10; ++byteCount) {
    assertValid(`memory32 offset 0 in ${byteCount} LEB bytes`, false, load32(paddedLeb(0n, byteCount)));
    assertValid(`memory64 offset 0 in ${byteCount} LEB bytes`, true, load64(paddedLeb(0n, byteCount)));
}
for (let byteCount = 5; byteCount <= 10; ++byteCount)
    assertValid(`memory32 offset 0xffffffff in ${byteCount} LEB bytes`, false, load32(paddedLeb(0xffffffffn, byteCount)));

// An offset that does not fit the memory's address type is still rejected.
assertInvalid("memory32 offset 2^32", false, load32(paddedLeb(0x100000000n, 5)));
assertInvalid("memory32 offset 2^64-1", false, load32(paddedLeb(0xffffffffffffffffn, 10)));
assertValid("memory64 offset 2^32", true, load64(paddedLeb(0x100000000n, 5)));
assertValid("memory64 offset 2^64-1", true, load64(paddedLeb(0xffffffffffffffffn, 10)));

// Eleven bytes cannot encode a u64.
assertInvalid("memory64 offset in 11 LEB bytes", true, load64([...paddedLeb(0n, 10).slice(0, 9), 0x80, 0x00]));
