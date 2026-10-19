//@ runDefaultWasm("-m", "--useWasmCustomPageSizes=1")

import * as assert from "../assert.js";

// A memory's page size must survive a structured clone. Only shared memories
// can be cloned, and a module may declare a shared memory with a one-byte page
// size, so the page size has to travel in the serialized record rather than
// being assumed to be the default.

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

// Shared memories need a maximum, so these limits are hasMax|shared|hasPageSize.
const sharedSmallPageMemory = new Uint8Array([
    ...header,
    ...section(0x05, [0x01, 0x0b, 0x00, 0x01, 0x00]),
    ...section(0x07, [0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00]),
]);

const memory = new WebAssembly.Instance(new WebAssembly.Module(sharedSmallPageMemory)).exports.mem;
assert.instanceof(memory.buffer, SharedArrayBuffer);

// The receiving agent probes the page size of what it got the only way the API
// exposes it: by which import slot the memory will link into.
$.agent.start(`
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
    const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    function importer(log2PageSize) {
        const imports = [0x01, 0x01, 0x65, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x0b, 0x00, 0x01, log2PageSize];
        return new Uint8Array([...header, ...section(0x02, imports)]);
    }
    $.agent.receiveBroadcast(function(received) {
        const results = [];
        for (const log2PageSize of [0, 16]) {
            try {
                new WebAssembly.Instance(new WebAssembly.Module(importer(log2PageSize)), { e: { mem: received } });
                results.push(log2PageSize + ":links");
            } catch (e) {
                results.push(log2PageSize + ":" + e.constructor.name);
            }
        }
        $.agent.report(results.join(","));
        $.agent.leaving();
    });
`);

$.agent.broadcast(memory);

let report;
while ((report = $.agent.getReport()) === null)
    $.agent.sleep(10);

// Had the page size been defaulted rather than serialized, these would be
// exactly reversed: the memory would arrive claiming 64 KiB pages.
assert.eq(report, "0:links,16:LinkError");
