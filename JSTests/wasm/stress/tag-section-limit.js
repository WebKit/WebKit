import * as assert from "../assert.js";

// https://www.w3.org/TR/wasm-js-api-2/#limits says a module may define at most
// 1,000,000 tags, and one that exceeds a limit must be rejected with a
// CompileError.
const maxTags = 1000000;

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

// Every tag refers to type 0, an empty function type, and takes two bytes: the
// exception attribute and the type index.
function moduleWithTags(count) {
    const countBytes = leb(count);
    const header = [
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
        0x0d, ...leb(countBytes.length + 2 * count), ...countBytes,
    ];
    const bytes = new Uint8Array(header.length + 2 * count);
    bytes.set(header);
    return bytes;
}

new WebAssembly.Module(moduleWithTags(maxTags));

assert.throws(
    () => new WebAssembly.Module(moduleWithTags(maxTags + 1)),
    WebAssembly.CompileError,
    `WebAssembly.Module doesn't parse at byte 21: Exception section's count is too big ${maxTags + 1} maximum ${maxTags}`
);
