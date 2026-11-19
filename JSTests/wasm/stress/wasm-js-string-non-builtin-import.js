//@ requireOptions("--useWasmJSStringBuiltins=true")
import * as assert from '../assert.js';

// (module
//   (func (import "wasm:js-string" "nonExistentFn") (result i32))
//   (func (export "run") (result i32)
//     call 0))
const userImport = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 127, 2, 32, 1, 14, 119, 97, 115, 109, 58, 106, 115, 45, 115, 116, 114, 105, 110, 103, 13, 110, 111, 110, 69, 120, 105, 115, 116, 101, 110, 116, 70, 110, 0, 0, 3, 2, 1, 0, 7, 7, 1, 3, 114, 117, 110, 0, 1, 10, 6, 1, 4, 0, 16, 0, 11]);

// (module
//   (func (import "wasm:js-string" "length") (param externref) (result i32))
//   (func (import "wasm:js-string" "nonExistentFn") (result i32))
//   (func (export "run") (param externref) (result i32)
//     local.get 0
//     call 0
//     call 1
//     i32.add))
const mixedImport = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 10, 2, 96, 1, 111, 1, 127, 96, 0, 1, 127, 2, 56, 2, 14, 119, 97, 115, 109, 58, 106, 115, 45, 115, 116, 114, 105, 110, 103, 6, 108, 101, 110, 103, 116, 104, 0, 0, 14, 119, 97, 115, 109, 58, 106, 115, 45, 115, 116, 114, 105, 110, 103, 13, 110, 111, 110, 69, 120, 105, 115, 116, 101, 110, 116, 70, 110, 0, 1, 3, 2, 1, 0, 7, 7, 1, 3, 114, 117, 110, 0, 2, 10, 11, 1, 9, 0, 32, 0, 16, 0, 16, 1, 106, 11]);

// (module
//   (func (import "wasm:js-string" "length") (param externref) (result i32))
//   (func (export "len") (param externref) (result i32)
//     local.get 0
//     call 0))
const lengthOnly = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 6, 1, 96, 1, 111, 1, 127, 2, 25, 1, 14, 119, 97, 115, 109, 58, 106, 115, 45, 115, 116, 114, 105, 110, 103, 6, 108, 101, 110, 103, 116, 104, 0, 0, 3, 2, 1, 0, 7, 7, 1, 3, 108, 101, 110, 0, 1, 10, 8, 1, 6, 0, 32, 0, 16, 0, 11]);

const compileOptions = { builtins: ["js-string"] };

{
    const module = new WebAssembly.Module(userImport, compileOptions);
    let called = false;
    const instance = new WebAssembly.Instance(module, {
        "wasm:js-string": {
            nonExistentFn() {
                called = true;
                return 7;
            },
        },
    });
    assert.eq(instance.exports.run(), 7);
    assert.eq(called, true);
    assert.throws(() => new WebAssembly.Instance(module, { "wasm:js-string": { } }), WebAssembly.LinkError, "must be callable");
}

{
    const module = new WebAssembly.Module(mixedImport, compileOptions);
    const importedNames = WebAssembly.Module.imports(module).map(entry => entry.name);
    assert.eq(importedNames.includes("nonExistentFn"), true);
    assert.eq(importedNames.includes("length"), false);
    const instance = new WebAssembly.Instance(module, {
        "wasm:js-string": {
            nonExistentFn() { return 7; },
        },
    });
    assert.eq(instance.exports.run("hi"), 9);
}

{
    const module = new WebAssembly.Module(lengthOnly, compileOptions);
    const instance = new WebAssembly.Instance(module, {
        "wasm:js-string": {
            length() { return 99; },
        },
    });
    assert.eq(instance.exports.len("hi"), 2);
}
