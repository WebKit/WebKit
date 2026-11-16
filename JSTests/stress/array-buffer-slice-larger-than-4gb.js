//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

const gib = 1024 * 1024 * 1024;
const size = 5 * gib;

for (const ArrayBufferClass of [ArrayBuffer, SharedArrayBuffer]) {
    const buffer = new ArrayBufferClass(size);
    shouldBe(buffer.byteLength, size);

    new Uint8Array(buffer).set([1, 2, 3, 4], size - 4);

    // Slicing only a window at the end keeps the copy small while still forcing the byte length
    // itself through the argument clamping.
    shouldBe(buffer.slice(size - 4).byteLength, 4);
    shouldBe([...new Uint8Array(buffer.slice(size - 4))].join(), "1,2,3,4");
    shouldBe(buffer.slice(-4).byteLength, 4);
    shouldBe(buffer.slice(size - 4, size).byteLength, 4);
    shouldBe(buffer.slice(size - 4, -1).byteLength, 3);
    shouldBe(buffer.slice(4 * gib, 4 * gib + 8).byteLength, 8);
    shouldBe(buffer.slice(size, size).byteLength, 0);
    shouldBe(buffer.slice(size + 1).byteLength, 0);
}
