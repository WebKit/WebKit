//@ requireOptions("--useWasmCompactImportSection=false")
import * as assert from "../assert.js";

function leb(n)
{
    const bytes = [];
    do {
        let byte = n & 0x7f;
        n >>>= 7;
        if (n)
            byte |= 0x80;
        bytes.push(byte);
    } while (n);
    return bytes;
}

function name(string)
{
    const bytes = [];
    for (let i = 0; i < string.length; ++i)
        bytes.push(string.charCodeAt(i));
    return [...leb(bytes.length), ...bytes];
}

function section(id, payload)
{
    return [id, ...leb(payload.length), ...payload];
}

function compactTwoFunctions()
{
    const type = section(1, [1, 0x60, 0, 1, 0x7f]);
    const imports = section(2, [
        1,
        ...name("env"),
        0x00,
        0x7e,
        0x00,
        0x00,
        2,
        ...name("a"),
        ...name("b"),
    ]);
    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, ...type, ...imports]);
}

assert.throws(() => new WebAssembly.Module(compactTwoFunctions()), WebAssembly.CompileError, "kind");