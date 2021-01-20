function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

if (typeof createBigInt32 !== 'undefined') {
    for (var i = -100000; i < 100000; ++i) {
        var bigInt32 = createBigInt32(String(i));
        var heapBigInt = createHeapBigInt(String(i));
        shouldBe(bigInt32 == heapBigInt, true);
    }
}
