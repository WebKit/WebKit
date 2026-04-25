function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (let i = 0; i < 1e3; i++) {
    transferArrayBuffer(new Uint8Array(2 ** 21).buffer);
}
fullGC();
shouldBe($vm.heapExtraMemorySize() < (2 ** 20 * 1e3), true);
