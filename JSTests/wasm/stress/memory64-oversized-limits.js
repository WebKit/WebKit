//@ skip if $addressBits <= 32
import * as assert from "../assert.js";

function leb128(value) {
    const bytes = [];
    let n = BigInt(value);
    do {
        let byte = Number(n & 0x7fn);
        n >>= 7n;
        if (n !== 0n)
            byte |= 0x80;
        bytes.push(byte);
    } while (n !== 0n);
    return bytes;
}

// A module whose only content is a memory with the given limits.
function moduleBytesWithMemoryLimits({ initial, maximum, is64bit = true }) {
    const hasMaximum = maximum !== undefined;
    const limitsFlags = (is64bit ? 0x04 : 0x00) | (hasMaximum ? 0x01 : 0x00); // bit 2: 64-bit index type, bit 0: has a maximum
    const limits = [limitsFlags, ...leb128(initial), ...(hasMaximum ? leb128(maximum) : [])];
    const memorySectionBody = [0x01, ...limits]; // 1 memory
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version
        0x05, ...leb128(memorySectionBody.length), ...memorySectionBody, // memory section
    ]);
}

// The JS API caps a memory64 memory's limits at 2**37 - 1 pages, far more than can be allocated.
// https://www.w3.org/TR/wasm-js-api-2/#limits
const maxMemory64Pages = (1n << 37n) - 1n;
const formerInvalidPageCountSentinel = (1n << 32n) - 1n;

// An initial page count that can be declared but not allocated compiles, and must be rejected at
// instantiation rather than truncated to an allocatable size.
for (const initial of [maxMemory64Pages, 1n << 32n, (1n << 32n) + 5n, formerInvalidPageCountSentinel]) {
    const module = new WebAssembly.Module(moduleBytesWithMemoryLimits({ initial }));
    assert.throws(() => new WebAssembly.Instance(module), RangeError, "Out of memory");
}

// A maximum is only a declaration: an unallocatable one doesn't prevent instantiation.
for (const maximum of [maxMemory64Pages, 1n << 32n, formerInvalidPageCountSentinel])
    new WebAssembly.Instance(new WebAssembly.Module(moduleBytesWithMemoryLimits({ initial: 1n, maximum })));

// Anything above the limit is invalid, and so is a memory32 above 2**16 pages.
for (const [limits, message] of [
    [{ initial: maxMemory64Pages + 1n }, `Memory's initial page count of ${maxMemory64Pages + 1n} is invalid`],
    [{ initial: 0n, maximum: maxMemory64Pages + 1n }, `Memory's maximum page count of ${maxMemory64Pages + 1n} is invalid`],
    [{ initial: 65537n, is64bit: false }, "Memory's initial page count of 65537 is invalid"],
])
    assert.throws(() => new WebAssembly.Module(moduleBytesWithMemoryLimits(limits)), WebAssembly.CompileError, message);
