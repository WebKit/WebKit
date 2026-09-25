//@ requireOptions("--useWasmJSStringBuiltins=0")

import * as assert from '../assert.js';

// Every field the compile options bag can carry sits behind its own JSC option, so whether the
// bag is looked at cannot be decided by any one of them. With the JS string builtins turned off,
// eagerValidate has to keep working.

// (module (func (export "f") (result i32)))  -- declares a result and returns nothing
const invalidBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

assert.falsy(WebAssembly.validate(invalidBytes));

assert.throws(() => new WebAssembly.Module(invalidBytes, { eagerValidate: true }), WebAssembly.CompileError, "");

// A badly typed field is still rejected, which is how a caller can tell the bag was read at all.
assert.throws(() => new WebAssembly.Module(invalidBytes, { eagerValidate: 7 }), TypeError, "eagerValidate");

async function testCompile() {
    let caught;
    try {
        await WebAssembly.compile(invalidBytes, { eagerValidate: true });
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
}

async function testInstantiate() {
    let caught;
    try {
        await WebAssembly.instantiate(invalidBytes, {}, { eagerValidate: true });
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
}

await assert.asyncTest(testCompile());
await assert.asyncTest(testInstantiate());
