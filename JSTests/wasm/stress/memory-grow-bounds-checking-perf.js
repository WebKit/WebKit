//@ requireOptions("--useWasmFastMemory=false", "--useExecutableAllocationFuzz=false")
import * as assert from "../assert.js";

function assertPreservesAndZeros() {
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 8 });
    const view = new Uint8Array(memory.buffer);
    view[0] = 0xab;
    view[65535] = 0xcd;
    const old = memory.grow(2);
    assert.eq(old, 1);
    assert.eq(memory.buffer.byteLength, 3 * 65536);
    const view2 = new Uint8Array(memory.buffer);
    assert.eq(view2[0], 0xab);
    assert.eq(view2[65535], 0xcd);
    assert.eq(view2[65536], 0);
    assert.eq(view2[3 * 65536 - 1], 0);
}

function assertManyGrows() {
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 64 });
    for (let i = 0; i < 63; i++)
        memory.grow(1);
    assert.eq(memory.buffer.byteLength, 64 * 65536);
    const view = new Uint8Array(memory.buffer);
    view[0] = 1;
    view[64 * 65536 - 1] = 2;
    assert.eq(view[0], 1);
    assert.eq(view[64 * 65536 - 1], 2);
}

function assertGrowFromZeroInitial() {
    const memory = new WebAssembly.Memory({ initial: 0, maximum: 8 });
    assert.eq(memory.buffer.byteLength, 0);
    assert.eq(memory.grow(1), 0);
    assert.eq(memory.buffer.byteLength, 65536);
    const view = new Uint8Array(memory.buffer);
    view[0] = 7;
    assert.eq(memory.grow(2), 1);
    assert.eq(memory.buffer.byteLength, 3 * 65536);
    assert.eq(new Uint8Array(memory.buffer)[0], 7);
}

function assertInitialEqualsMaximum() {
    const memory = new WebAssembly.Memory({ initial: 2, maximum: 2 });
    assert.eq(memory.buffer.byteLength, 2 * 65536);
    assert.throws(() => memory.grow(1), RangeError, "maximum");
}

function assertDestroyAfterFullGrow() {
    let memory = new WebAssembly.Memory({ initial: 1, maximum: 4 });
    while (memory.buffer.byteLength < 4 * 65536)
        memory.grow(1);
    assert.eq(memory.buffer.byteLength, 4 * 65536);
    memory = null;
    gc();
    const again = new WebAssembly.Memory({ initial: 1, maximum: 4 });
    assert.eq(again.grow(1), 1);
    assert.eq(again.buffer.byteLength, 2 * 65536);
}

function assertOOBAfterPartialGrow() {
    const bytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
        0x02, 0x0b, 0x01, 0x02, 0x65, 0x6e, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x01,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x67, 0x65, 0x74, 0x00, 0x00,
        0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x0b,
    ]);
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 4 });
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), { en: { mem: memory } });
    assert.throws(() => instance.exports.get(65536), WebAssembly.RuntimeError, "Out of bounds");
    memory.grow(1);
    assert.eq(instance.exports.get(65536), 0);
    assert.throws(() => instance.exports.get(4 * 65536), WebAssembly.RuntimeError, "Out of bounds");
}

function assertGrowWithoutMaximumStillWorks() {
    const memory = new WebAssembly.Memory({ initial: 1 });
    const view = new Uint8Array(memory.buffer);
    view[0] = 9;
    assert.eq(memory.grow(1), 1);
    assert.eq(new Uint8Array(memory.buffer)[0], 9);
    assert.eq(memory.buffer.byteLength, 2 * 65536);
}

function assertManyGrowsWithoutMaximum() {
    const memory = new WebAssembly.Memory({ initial: 1 });
    for (let i = 0; i < 63; i++)
        memory.grow(1);
    assert.eq(memory.buffer.byteLength, 64 * 65536);
    const view = new Uint8Array(memory.buffer);
    view[0] = 1;
    view[64 * 65536 - 1] = 2;
    assert.eq(view[0], 1);
    assert.eq(view[64 * 65536 - 1], 2);
}

function assertGrowFromZeroInitialWithoutMaximum() {
    const memory = new WebAssembly.Memory({ initial: 0 });
    assert.eq(memory.buffer.byteLength, 0);
    assert.eq(memory.grow(1), 0);
    assert.eq(memory.buffer.byteLength, 65536);
    const view = new Uint8Array(memory.buffer);
    view[0] = 7;
    assert.eq(memory.grow(2), 1);
    assert.eq(memory.buffer.byteLength, 3 * 65536);
    assert.eq(new Uint8Array(memory.buffer)[0], 7);
}

function assertOOBAfterPartialGrowWithoutMaximum() {
    const bytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
        0x02, 0x0b, 0x01, 0x02, 0x65, 0x6e, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x01,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 0x67, 0x65, 0x74, 0x00, 0x00,
        0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x28, 0x02, 0x00, 0x0b,
    ]);
    const memory = new WebAssembly.Memory({ initial: 1 });
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), { en: { mem: memory } });
    assert.throws(() => instance.exports.get(65536), WebAssembly.RuntimeError, "Out of bounds");
    memory.grow(1);
    assert.eq(instance.exports.get(65536), 0);
    assert.throws(() => instance.exports.get(4 * 65536), WebAssembly.RuntimeError, "Out of bounds");
}

assertPreservesAndZeros();
assertManyGrows();
assertGrowFromZeroInitial();
assertInitialEqualsMaximum();
assertDestroyAfterFullGrow();
assertOOBAfterPartialGrow();
assertGrowWithoutMaximumStillWorks();
assertManyGrowsWithoutMaximum();
assertGrowFromZeroInitialWithoutMaximum();
assertOOBAfterPartialGrowWithoutMaximum();
