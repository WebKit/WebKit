function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe([10n].toString(), `10`);
shouldBe([10n, 20n, 30n].toString(), `10,20,30`);
shouldBe([createHeapBigInt(10n)].toString(), `10`);
shouldBe([10n, createHeapBigInt(20n), 30n].toString(), `10,20,30`);
shouldBe([createHeapBigInt(10n), createHeapBigInt(20n), 30n].toString(), `10,20,30`);
