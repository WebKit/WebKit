//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=0")

import * as assert from "../assert.js";

// Parsing of the custom-page-sizes limits encoding: bit 3 of the flags byte
// says a u32 page size exponent trails the min and max.

const customPageSizesEnabled = !!jscOptions().useWasmCustomPageSizes;

const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];

function section(id, bytes) {
    return [id, bytes.length, ...bytes];
}

// A memory section holding one memory with the given limits bytes.
function memoryModule(limits) {
    return new Uint8Array([...header, ...section(0x05, [0x01, ...limits])]);
}

// A table section holding one funcref table with the given limits bytes.
function tableModule(limits) {
    return new Uint8Array([...header, ...section(0x04, [0x01, 0x70, ...limits])]);
}

function parses(bytes) {
    new WebAssembly.Module(bytes);
}

function failsToParse(bytes, message) {
    assert.throws(() => new WebAssembly.Module(bytes), WebAssembly.CompileError, message);
}

// Flags 0x08-0x0f: every combination of hasMax, shared, i64 alongside a page
// size. Shared requires a maximum, and i64 widens min/max to u64, but the
// exponent is a u32 in every case and always comes last.
function limitsFor(flags, log2PageSize) {
    const hasMax = flags & 0x01;
    const isShared = flags & 0x02;
    // Shared memory without a maximum is rejected before the page size is read.
    if (isShared && !hasMax)
        return null;
    const limits = [flags, 0x01];
    if (hasMax)
        limits.push(0x01);
    limits.push(log2PageSize);
    return limits;
}

for (let flags = 0x08; flags <= 0x0f; ++flags) {
    if (!customPageSizesEnabled) {
        // Rejected for the disabled feature before any later check, so even
        // the otherwise-malformed shared-without-maximum bytes fail this way.
        failsToParse(memoryModule([flags, 0x01, 0x01, 0x10]), "custom page sizes are not enabled");
        continue;
    }
    const limits = limitsFor(flags, 16);
    if (limits === null)
        continue;
    parses(memoryModule(limits));
}

// Bit 3 on a table is never valid, feature on or off. Tables reject it as an
// unknown flag rather than as a disabled feature.
for (let flags = 0x08; flags <= 0x0f; ++flags)
    failsToParse(tableModule([flags, 0x01, 0x01]), "resizable limits flag are not valid");

if (customPageSizesEnabled) {
    // The exponent must leave 2**exponent representable.
    failsToParse(memoryModule([0x08, 0x00, 0x40]), "page size 2**64 is too large");
    failsToParse(memoryModule([0x08, 0x00, 0x41]), "page size 2**65 is too large");

    // 63 is the largest exponent this commit accepts. Validation narrowing it
    // to 0 or 16 comes later, at which point this becomes a CompileError.
    parses(memoryModule([0x08, 0x00, 0x3f]));

    // Page size 1 and the default both parse.
    parses(memoryModule([0x08, 0x01, 0x00]));
    parses(memoryModule([0x08, 0x01, 0x10]));

    // The exponent trails the maximum rather than preceding it. These two
    // modules hold the same three bytes in the two possible orders, and only
    // the second reads 0x40 as the maximum instead of as the exponent.
    failsToParse(memoryModule([0x09, 0x01, 0x01, 0x40]), "page size 2**64 is too large");
    parses(memoryModule([0x09, 0x01, 0x40, 0x01]));
}
