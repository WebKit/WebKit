function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var zero1 = createHeapBigInt(0n);
var zero2 = createHeapBigInt(-1n);
zero2++;

shouldBe(zero1 === zero2, true);
