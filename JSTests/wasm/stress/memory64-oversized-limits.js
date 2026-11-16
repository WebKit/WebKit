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

// A memory's declared limits are bounded by its address type: an i64 memory may declare up to 2**48
// pages, the whole of its 2**64-byte address space, and an i32 memory 2**16. A declaration is only a
// declaration, so anything within the bound compiles no matter how far past what a port could map;
// what cannot be allocated is refused at instantiation instead.
const maxMemory64Pages = 1n << 48n;
const pageSize = 65536;

// An initial page count is checked against that bound and nothing else, so every count up to it
// compiles, and instantiation must honor it verbatim rather than clamp it to a smaller size.
for (const initial of [65537n, 262144n]) {
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

// No port can host the largest declarable initial size, so that one compiles and then always fails to
// instantiate. It must not wrap around to a size that looks allocatable.
{
    const module = new WebAssembly.Module(moduleBytesWithMemoryLimits({ initial: maxMemory64Pages }));
    assert.throws(() => new WebAssembly.Instance(module), RangeError, "Out of memory");
}

// A maximum is only a declaration, so even one at the bound costs nothing to instantiate: the memory
// is created at its initial size.
for (const maximum of [65537n, 262144n, 1n << 32n, (1n << 37n) - 1n, maxMemory64Pages]) {
    const { mem } = new WebAssembly.Instance(new WebAssembly.Module(moduleBytesWithMemoryLimits({ initial: 1n, maximum }))).exports;
    assert.eq(mem.buffer.byteLength, pageSize);
}

// Anything above the bound is invalid, including the UINT64_MAX that PageCount reserves to mean "no
// page count". A memory32 above 2**16 pages is invalid too.
const pageCountSentinel = (1n << 64n) - 1n;
for (const [limits, message] of [
    [{ initial: maxMemory64Pages + 1n }, `Memory's initial page count of ${maxMemory64Pages + 1n} is invalid`],
    [{ initial: 0n, maximum: maxMemory64Pages + 1n }, `Memory's maximum page count of ${maxMemory64Pages + 1n} is invalid`],
    [{ initial: pageCountSentinel }, `Memory's initial page count of ${pageCountSentinel} is invalid`],
    [{ initial: 0n, maximum: pageCountSentinel }, `Memory's maximum page count of ${pageCountSentinel} is invalid`],
    [{ initial: 65537n, is64bit: false }, "Memory's initial page count of 65537 is invalid"],
])
    assert.throws(() => new WebAssembly.Module(moduleBytesWithMemoryLimits(limits)), WebAssembly.CompileError, message);
