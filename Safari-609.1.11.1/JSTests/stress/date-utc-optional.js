function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var i = 0; i < 1e5; ++i) {
    shouldBe(Number.isNaN(Date.UTC()), true);
    shouldBe(Date.UTC(2018), 1514764800000);
    shouldBe(Date.UTC(2018, 1), 1517443200000);
    shouldBe(Date.UTC(2018, 1, 2), 1517529600000);
    shouldBe(Date.UTC(2018, 1, 2, 3), 1517540400000);
    shouldBe(Date.UTC(2018, 1, 2, 3, 4), 1517540640000);
    shouldBe(Date.UTC(2018, 1, 2, 3, 4, 5), 1517540645000);
    shouldBe(Date.UTC(2018, 1, 2, 3, 4, 5, 6), 1517540645006);
    shouldBe(Date.UTC(2018, 1, 2, 3, 4, 5, 6, 7), 1517540645006);
}
