//@ skip if $addressBits <= 32
//@ requireOptions("--useWasmJSStringBuiltins=true")

import * as assert from '../assert.js';

// Import names are raw UTF-8 in the module while importedStringConstants arrives as a JS string, so
// the two match only when the JS string is compared in its UTF-8 encoding.

function uleb(bytes, value) {
    do {
        const byte = value & 0x7f;
        value >>>= 7;
        bytes.push(value ? byte | 0x80 : byte);
    } while (value);
}

function name(bytes, utf8) {
    uleb(bytes, utf8.length);
    bytes.push(...utf8);
}

// (module (import <moduleName> "hello" (global externref)) (export "g" (global 0)))
// An immutable externref global is the only import shape eligible to be a string constant.
function moduleWithGlobalImport(moduleName) {
    const imports = [1];
    name(imports, moduleName);
    name(imports, [0x68, 0x65, 0x6c, 0x6c, 0x6f]); // "hello"
    imports.push(0x03, 0x6f, 0x00);

    const exports = [1];
    name(exports, [0x67]); // "g"
    exports.push(0x03, 0x00);

    const bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    bytes.push(0x02);
    uleb(bytes, imports.length);
    bytes.push(...imports);
    bytes.push(0x07);
    uleb(bytes, exports.length);
    bytes.push(...exports);
    return new Uint8Array(bytes);
}

const cafeUTF8 = [0x63, 0x61, 0x66, 0xc3, 0xa9]; // "caf\u00e9"
const replacementUTF8 = [0xef, 0xbf, 0xbd]; // "\ufffd"

function testMatch(moduleName, importedStringConstants) {
    const module = new WebAssembly.Module(moduleWithGlobalImport(moduleName), { importedStringConstants });
    // The engine supplies a string constant itself, so it is hidden from imports() and needs no import object.
    assert.eq(WebAssembly.Module.imports(module).length, 0);
    assert.eq(new WebAssembly.Instance(module, {}).exports.g.value, "hello");
}

function testNoMatch(moduleName, importedStringConstants) {
    const module = new WebAssembly.Module(moduleWithGlobalImport(moduleName), { importedStringConstants });
    assert.eq(WebAssembly.Module.imports(module).length, 1);
    assert.throws(() => new WebAssembly.Instance(module, {}), TypeError, "must be an object");
}

testMatch(cafeUTF8, "caf\u00e9");
testMatch(replacementUTF8, "\ufffd");
testMatch([], "");

// A lone surrogate has no UTF-8 encoding, and import names are required to be well-formed UTF-8, so
// it can never name an import.
testNoMatch(replacementUTF8, "\ud800");
testNoMatch(cafeUTF8, "caf\ud800");

testNoMatch(cafeUTF8, "cafe");
testNoMatch(cafeUTF8, "caf\u00e9x");

// Builtin set names take the same encoding path, and none of these name a registered set.
for (const builtin of ["caf\u00e9", "\ud800", "\ufffd"]) {
    const module = new WebAssembly.Module(moduleWithGlobalImport(cafeUTF8), { builtins: [builtin] });
    assert.eq(WebAssembly.Module.imports(module).length, 1);
}
