//@ requireOptions("--useWasmJSTypes=1", "--useSharedArrayBuffer=1")
//@ skip if $addressBits <= 32
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// The minimum a memory or table reflects is its current size, not the size it was declared with, so
// that feeding a reflected type back into the constructor reproduces the object it came from.

function check(object, expected) {
    const type = object.type();
    assert.eq(type.minimum, expected.minimum);
    assert.eq(type.maximum, expected.maximum);
    assert.eq(type.address, expected.address);
}

for (const shared of [false, true]) {
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 10, shared });
    check(memory, { minimum: 1, maximum: 10, address: "i32" });
    memory.grow(3);
    check(memory, { minimum: 4, maximum: 10, address: "i32" });
    assert.eq(memory.buffer.byteLength, 4 * 65536);
    assert.eq(new WebAssembly.Memory(memory.type()).buffer.byteLength, memory.buffer.byteLength);

    const memory64 = new WebAssembly.Memory({ address: "i64", initial: 1n, maximum: 10n, shared });
    check(memory64, { minimum: 1n, maximum: 10n, address: "i64" });
    memory64.grow(3n);
    check(memory64, { minimum: 4n, maximum: 10n, address: "i64" });
    assert.eq(new WebAssembly.Memory(memory64.type()).buffer.byteLength, memory64.buffer.byteLength);
}

// A memory grown by the wasm memory.grow instruction reflects the new size too.
for (const addressType of ["i32", "i64"]) {
    const numOrBig = (val) => addressType == "i32" ? Number(val) : BigInt(val);
    const { grow, memory } = (await instantiate(`
(module
    (memory (export "memory") ${addressType} 1 10)
    (func (export "grow") (param ${addressType}) (result ${addressType})
        (memory.grow (local.get 0)))
)`, {}, { memory64: true })).exports;
    check(memory, { minimum: numOrBig(1), maximum: numOrBig(10), address: addressType });
    assert.eq(grow(numOrBig(2)), numOrBig(1));
    check(memory, { minimum: numOrBig(3), maximum: numOrBig(10), address: addressType });
}

// A memory with no declared maximum still reflects its current size.
{
    const memory = new WebAssembly.Memory({ initial: 2 });
    check(memory, { minimum: 2, maximum: undefined, address: "i32" });
    memory.grow(1);
    check(memory, { minimum: 3, maximum: undefined, address: "i32" });
}
