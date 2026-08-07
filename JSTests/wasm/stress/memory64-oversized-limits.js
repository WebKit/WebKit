//@ memoryHog!
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

// A module whose only content is a memory with the given limits, exported as "mem".
function moduleBytesWithMemoryLimits({ initial, maximum, is64bit = true }) {
    const hasMaximum = maximum !== undefined;
    const limitsFlags = (is64bit ? 0x04 : 0x00) | (hasMaximum ? 0x01 : 0x00); // bit 2: 64-bit index type, bit 0: has a maximum
    const limits = [limitsFlags, ...leb128(initial), ...(hasMaximum ? leb128(maximum) : [])];
    const memorySectionBody = [0x01, ...limits]; // 1 memory
    const exportSectionBody = [0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00]; // 1 export: "mem" names memory 0
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version
        0x05, ...leb128(memorySectionBody.length), ...memorySectionBody, // memory section
        0x07, ...leb128(exportSectionBody.length), ...exportSectionBody, // export section
    ]);
}

// The JS API caps the limits a memory may declare at 262144 pages (16 GiB) for a 64-bit memory and
// 65536 pages (4 GiB) for a 32-bit one. https://www.w3.org/TR/wasm-js-api-2/#limits
const maxMemory64Pages = 262144n;
const pageSize = 65536;

// An initial page count is checked against that cap and nothing else, so every count up to it
// compiles, and instantiation must honor it verbatim rather than clamp it to a smaller size.
for (const initial of [65537n, maxMemory64Pages]) {
    const module = new WebAssembly.Module(moduleBytesWithMemoryLimits({ initial }));
    let memory;
    try {
        memory = new WebAssembly.Instance(module).exports.mem;
    } catch (e) {
        // A port whose address space cannot host this much refuses cleanly instead.
        assert.truthy(e instanceof RangeError && e.message === "Out of memory", `expected an out of memory RangeError, got ${e}`);
        continue;
    }
    assert.eq(memory.buffer.byteLength, Number(initial) * pageSize);
}

// A maximum is only a declaration, so even one at the cap costs nothing to instantiate: the memory
// is created at its initial size.
for (const maximum of [65537n, maxMemory64Pages]) {
    const { mem } = new WebAssembly.Instance(new WebAssembly.Module(moduleBytesWithMemoryLimits({ initial: 1n, maximum }))).exports;
    assert.eq(mem.buffer.byteLength, pageSize);
}

// Anything above the cap is invalid, including page counts that used to be declarable when the bound
// was PageCount's own 2**37-1, and the UINT64_MAX that PageCount reserves to mean "no page count".
// A memory32 above 2**16 pages is invalid too.
const pageCountSentinel = (1n << 64n) - 1n;
for (const [limits, message] of [
    [{ initial: maxMemory64Pages + 1n }, `Memory's initial page count of ${maxMemory64Pages + 1n} is invalid`],
    [{ initial: 0n, maximum: maxMemory64Pages + 1n }, `Memory's maximum page count of ${maxMemory64Pages + 1n} is invalid`],
    [{ initial: 0n, maximum: (1n << 32n) - 1n }, `Memory's maximum page count of ${(1n << 32n) - 1n} is invalid`],
    [{ initial: 0n, maximum: 1n << 32n }, `Memory's maximum page count of ${1n << 32n} is invalid`],
    [{ initial: 0n, maximum: (1n << 37n) - 1n }, `Memory's maximum page count of ${(1n << 37n) - 1n} is invalid`],
    [{ initial: pageCountSentinel }, `Memory's initial page count of ${pageCountSentinel} is invalid`],
    [{ initial: 0n, maximum: pageCountSentinel }, `Memory's maximum page count of ${pageCountSentinel} is invalid`],
    [{ initial: 65537n, is64bit: false }, "Memory's initial page count of 65537 is invalid"],
])
    assert.throws(() => new WebAssembly.Module(moduleBytesWithMemoryLimits(limits)), WebAssembly.CompileError, message);
