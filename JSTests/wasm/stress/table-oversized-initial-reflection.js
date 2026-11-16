//@ skip if $addressBits <= 32
//@ requireOptions("--useWasmJSTypes=true")
import * as assert from "../assert.js";

// A table may declare a size larger than this implementation can create. That is a compile-time
// success and an instantiation-time failure, so the type reflected by Module.imports() and
// Module.exports() has to be the size the module declared, not one clamped to what is creatable.

const SECTION_TYPE = 1;
const SECTION_IMPORT = 2;
const SECTION_TABLE = 4;
const SECTION_EXPORT = 7;

function leb(value) {
    let bytes = [];
    do {
        let byte = Number(value & 0x7fn);
        value >>= 7n;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(BigInt(payload.length)), ...payload];
}

const name = (text) => [text.length, ...Array.from(text, (c) => c.charCodeAt(0))];

// flags: 0x00 min only i32, 0x04 min only i64
function importedTable(initial, isTable64) {
    let bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    bytes.push(...section(SECTION_TYPE, [0x00]));
    bytes.push(...section(SECTION_IMPORT, [0x01, ...name("m"), ...name("t"), 0x01, 0x70, isTable64 ? 0x04 : 0x00, ...leb(initial)]));
    return new Uint8Array(bytes);
}

function exportedTable(initial, isTable64) {
    let bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    bytes.push(...section(SECTION_TYPE, [0x00]));
    bytes.push(...section(SECTION_TABLE, [0x01, 0x70, isTable64 ? 0x04 : 0x00, ...leb(initial)]));
    bytes.push(...section(SECTION_EXPORT, [0x01, ...name("t"), 0x01, 0x00]));
    return new Uint8Array(bytes);
}

function checkMinimum(bytes, expected, description) {
    const module = new WebAssembly.Module(bytes);
    const imports = WebAssembly.Module.imports(module);
    const exports = WebAssembly.Module.exports(module);
    const descriptor = imports.length ? imports[0] : exports[0];
    assert.eq(descriptor.kind, "table");
    assert.eq(descriptor.type.minimum, expected, description);
}

// Sizes above the creatable limit keep their declared value.
checkMinimum(importedTable(1099511627776n, true), 1099511627776n, "table64 import, 2^40");
checkMinimum(importedTable(4294967301n, true), 4294967301n, "table64 import, 2^32 + 5");
checkMinimum(importedTable(18446744073709551615n, true), 18446744073709551615n, "table64 import, 2^64 - 1");
checkMinimum(importedTable(20000000n, false), 20000000, "table32 import, 20000000");
checkMinimum(exportedTable(4294967301n, true), 4294967301n, "table64 definition, 2^32 + 5");
checkMinimum(exportedTable(20000000n, false), 20000000, "table32 definition, 20000000");

// Sizes at or below it are unaffected.
checkMinimum(importedTable(10n, true), 10n, "table64 import, 10");
checkMinimum(importedTable(10n, false), 10, "table32 import, 10");
checkMinimum(exportedTable(9999999n, false), 9999999, "table32 definition, 9999999");

// Such a table still cannot be created, and no JS table can satisfy the import.
for (const isTable64 of [false, true]) {
    const initial = isTable64 ? 4294967301n : 20000000n;
    assert.throws(() => new WebAssembly.Instance(new WebAssembly.Module(exportedTable(initial, isTable64))),
        WebAssembly.LinkError, "couldn't create Table");

    const table = new WebAssembly.Table({ element: "anyfunc", initial: isTable64 ? 10n : 10, address: isTable64 ? "i64" : "i32" });
    assert.throws(() => new WebAssembly.Instance(new WebAssembly.Module(importedTable(initial, isTable64)), { m: { t: table } }),
        WebAssembly.LinkError, "Table import m:t provided an 'initial' that is too small");
}
