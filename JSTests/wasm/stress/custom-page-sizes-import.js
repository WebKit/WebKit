//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")
//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=0")

import * as assert from "../assert.js";

// Memory type matching requires exactly equal page sizes. There is no subtyping
// between page sizes, so importing the wrong one is a link error.

const customPageSizesEnabled = !!jscOptions().useWasmCustomPageSizes;

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

// A memory's limits bytes. Omitting log2PageSize omits the page size entirely,
// which is the pre-proposal encoding and means the default 64 KiB.
function limits(log2PageSize) {
    if (log2PageSize === undefined)
        return [0x00, 0x00];
    return [0x08, 0x00, log2PageSize];
}

// Defines a memory and exports it as "mem".
function exporterModule(log2PageSize) {
    const memory = [0x01, ...limits(log2PageSize)];
    const exports = [0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00];
    return new Uint8Array([...header, ...section(0x05, memory), ...section(0x07, exports)]);
}

// Imports "e"."mem" as a memory with the given page size.
function importerModule(log2PageSize) {
    const imports = [0x01, 0x01, 0x65, 0x03, 0x6d, 0x65, 0x6d, 0x02, ...limits(log2PageSize)];
    return new Uint8Array([...header, ...section(0x02, imports)]);
}

function exportMemory(log2PageSize) {
    const instance = new WebAssembly.Instance(new WebAssembly.Module(exporterModule(log2PageSize)));
    return instance.exports.mem;
}

function links(memory, log2PageSize) {
    new WebAssembly.Instance(new WebAssembly.Module(importerModule(log2PageSize)), { e: { mem: memory } });
}

function failsToLink(memory, log2PageSize) {
    assert.throws(
        () => new WebAssembly.Instance(new WebAssembly.Module(importerModule(log2PageSize)), { e: { mem: memory } }),
        WebAssembly.LinkError,
        "provided a 'pagesize' that is different from the module's declared 'pagesize'");
}

// A memory declared with no explicit page size satisfies an import declared the
// same way. This is the pre-proposal encoding on both sides.
links(exportMemory(undefined), undefined);

// A JS-created memory satisfies an import with no explicit page size, since
// both are the default 64 KiB.
links(new WebAssembly.Memory({ initial: 0 }), undefined);

if (!customPageSizesEnabled) {
    // The page size byte is rejected while parsing, so none of the checks below
    // can be reached.
    assert.throws(() => new WebAssembly.Module(importerModule(16)), WebAssembly.CompileError,
        "custom page sizes are not enabled");
}

if (customPageSizesEnabled) {
    // Equal page sizes link, for each of the two valid ones.
    for (const log2PageSize of [0, 16])
        links(exportMemory(log2PageSize), log2PageSize);

    // Unequal page sizes do not, in either direction.
    failsToLink(exportMemory(0), 16);
    failsToLink(exportMemory(16), 0);

    // An explicit (pagesize 65536) import is satisfied by a memory declared with
    // no explicit page size: the encodings differ but both mean exponent 16.
    links(exportMemory(undefined), 16);
    links(exportMemory(16), undefined);

    // A JS-created memory is always 64 KiB, since the JS API has no page size.
    // So it satisfies a (pagesize 65536) slot and never a (pagesize 1) one.
    links(new WebAssembly.Memory({ initial: 0 }), 16);
    failsToLink(new WebAssembly.Memory({ initial: 0 }), 0);
}
