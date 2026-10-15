//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault("--useConcurrentJIT=0")
//@ runDefault("--useConcurrentJIT=0", "--useFTLJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`expected ${errorType.name} but got ${error}`);
}

function set(target, source, offset) {
    return target.set(source, offset);
}
noInline(set);

const gib = 1024 * 1024 * 1024;
const length = 5 * gib;
const target = new Uint8Array(length);
const source = new Uint8Array([1, 2, 3, 4]);

for (let i = 0; i < testLoopCount; i++)
    set(target, source, i & 1023);

shouldThrow(() => set(target, source, -1), RangeError);
shouldThrow(() => set(target, source, -4), RangeError);
shouldThrow(() => set(target, source, -(2 ** 31)), RangeError);
shouldBe(target[2 ** 32 - 4], 0);
shouldBe(target[2 ** 32 - 1], 0);
shouldBe(target[2 ** 31], 0);

set(target, source, 2 ** 31 - 1);
shouldBe(target[2 ** 31 - 2], 0);
shouldBe(target[2 ** 31 - 1], 1);
shouldBe(target[2 ** 31 + 2], 4);
shouldBe(target[2 ** 31 + 3], 0);

set(target, source, length - 4);
shouldBe(target[length - 5], 0);
shouldBe(target[length - 4], 1);
shouldBe(target[length - 1], 4);
shouldThrow(() => set(target, source, length - 3), RangeError);
