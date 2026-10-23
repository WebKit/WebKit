//@ requireOptions("--useWasmCustomPageSizes=1", "--useWasmJSTypes=1", "--useWasmMemoryToBufferAPIs=1")

import * as assert from "../assert.js";

// The JS API has no page size: WebAssembly.Memory always builds a 64 KiB-paged
// memory, and a module-defined memory of another page size is still reachable
// from JS through its exported buffer.

const pageSize = 64 * 1024;

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

// A memory's limits bytes. Bit 3 of the flags says an explicit page size follows,
// encoded as the exponent in 2**p.
function limits(initial, maximum, log2PageSize) {
    const flags = (maximum === undefined ? 0x00 : 0x01) | (log2PageSize === undefined ? 0x00 : 0x08);
    const bytes = [flags, ...varuint(initial)];
    if (maximum !== undefined)
        bytes.push(...varuint(maximum));
    if (log2PageSize !== undefined)
        bytes.push(log2PageSize);
    return bytes;
}

// Defines a memory and exports it as "mem".
function exporterModule(initial, maximum, log2PageSize) {
    const memory = [0x01, ...limits(initial, maximum, log2PageSize)];
    const exports = [0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00];
    return new Uint8Array([...header, ...section(0x05, memory), ...section(0x07, exports)]);
}

// Imports "e"."mem" as a memory with the given page size.
function importerModule(initial, log2PageSize) {
    const imports = [0x01, 0x01, 0x65, 0x03, 0x6d, 0x65, 0x6d, 0x02, ...limits(initial, undefined, log2PageSize)];
    return new Uint8Array([...header, ...section(0x02, imports)]);
}

function exportMemory(initial, maximum, log2PageSize) {
    const module = new WebAssembly.Module(exporterModule(initial, maximum, log2PageSize));
    return new WebAssembly.Instance(module).exports.mem;
}

// The constructor is always 64 KiB-paged.
{
    const memory = new WebAssembly.Memory({ initial: 1 });
    assert.eq(memory.buffer.byteLength, pageSize);
}

// A "pagesize" property is ignored rather than rejected, the way an unknown
// WebIDL dictionary member is. The JS API spec defines no such member, so code
// written against a future one must not appear to work.
{
    const memory = new WebAssembly.Memory({ initial: 1, pagesize: 1 });
    assert.eq(memory.buffer.byteLength, pageSize);
}

// So a JS-created memory never satisfies a (pagesize 1) import. This is the
// JS-API-facing statement of the page-size equality rule.
{
    const memory = new WebAssembly.Memory({ initial: 1, pagesize: 1 });
    assert.throws(
        () => new WebAssembly.Instance(new WebAssembly.Module(importerModule(1, 0)), { e: { mem: memory } }),
        WebAssembly.LinkError,
        "provided a 'pagesize' that is different from the module's declared 'pagesize'");

    // It does satisfy a (pagesize 65536) one.
    new WebAssembly.Instance(new WebAssembly.Module(importerModule(1, 16)), { e: { mem: memory } });
}

// A module-defined small-page memory exported to JS has a buffer of its exact
// byte size, which need not be a multiple of 64 KiB.
{
    const memory = exportMemory(3, undefined, 0);
    assert.eq(memory.buffer.byteLength, 3);

    const bytes = new Uint8Array(memory.buffer);
    assert.eq(bytes.length, 3);
    bytes[0] = 1;
    bytes[2] = 255;
    assert.eq(bytes[0], 1);
    assert.eq(bytes[1], 0);
    assert.eq(bytes[2], 255);
}

// Growing such a memory from JS counts in its own pages, i.e. bytes.
{
    const memory = exportMemory(3, 10, 0);
    assert.eq(memory.grow(2), 3);
    assert.eq(memory.buffer.byteLength, 5);
}

// Resizing a wasm-backed buffer rejects lengths that are not a multiple of that
// memory's page size, so a one-byte-paged memory accepts any length.
{
    const memory = exportMemory(3, 10, 0);
    const buffer = memory.toResizableBuffer();
    assert.eq(buffer.byteLength, 3);
    assert.eq(buffer.maxByteLength, 10);

    buffer.resize(7);
    assert.eq(buffer.byteLength, 7);
    new Uint8Array(buffer)[6] = 9;
    assert.eq(new Uint8Array(buffer)[6], 9);

    assert.throws(() => buffer.resize(6), RangeError, "Cannot shrink WebAssembly memory");
    assert.throws(() => buffer.resize(11), RangeError, "ArrayBuffer resize failed with new byte length 11");
}

// A 64 KiB memory's buffer still only resizes by whole pages.
{
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 4 });
    const buffer = memory.toResizableBuffer();
    buffer.resize(2 * pageSize);
    assert.eq(buffer.byteLength, 2 * pageSize);
    assert.throws(() => buffer.resize(3 * pageSize - 1), RangeError,
        "WebAssembly memory cannot be resized to new byte length 196607 because it is not a multiple of 65536");
}

// Memory.prototype.type() reports the current size in the memory's own pages.
{
    const memory = exportMemory(3, 10, 0);
    assert.eq(JSON.stringify(memory.type()), JSON.stringify({ maximum: 10, minimum: 3, shared: false, address: "i32" }));
}

// A JS-created memory's type is unchanged, and gains no page size field.
{
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 2 });
    assert.eq(JSON.stringify(memory.type()), JSON.stringify({ maximum: 2, minimum: 1, shared: false, address: "i32" }));
    assert.eq(Object.keys(new WebAssembly.Memory({ initial: 1 }).type()).join(), "minimum,shared,address");
}

// Module type reflection describes a module's declared types, so a small-page
// memory is a real case there. The js-types spec defines no field for a page
// size, so the descriptor keeps today's shape and does not report one.
{
    const memory = [0x01, ...limits(3, 10, 0)];
    const imports = [0x01, 0x01, 0x65, 0x03, 0x69, 0x6d, 0x70, 0x02, ...limits(5, undefined, 0)];
    const exports = [0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x01];
    const module = new WebAssembly.Module(new Uint8Array([...header,
        ...section(0x02, imports), ...section(0x05, memory), ...section(0x07, exports)]));

    const imported = WebAssembly.Module.imports(module)[0];
    assert.eq(imported.kind, "memory");
    assert.eq(Object.keys(imported.type).join(), "minimum,shared,address");
    assert.eq(imported.type.minimum, 5);

    const exported = WebAssembly.Module.exports(module)[0];
    assert.eq(exported.kind, "memory");
    assert.eq(Object.keys(exported.type).join(), "maximum,minimum,shared,address");
    assert.eq(exported.type.minimum, 3);
    assert.eq(exported.type.maximum, 10);
}
