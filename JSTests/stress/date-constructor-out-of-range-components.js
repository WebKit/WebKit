function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(Number.isNaN(new Date(1000000000, 0, 1).valueOf()), true);
shouldBe(Number.isNaN(new Date(-1000000000, 0, 1).valueOf()), true);
shouldBe(Number.isNaN(Date.UTC(1000000000, 0, 1)), true);
shouldBe(Date.UTC(275760, 8, 13), 8.64e15);
shouldBe(new Date(2020, 0, 1).getFullYear(), 2020);
shouldBe(Date.UTC(2020, 0, 1), Date.parse("2020-01-01T00:00:00.000Z"));
shouldBe(new Date(8.64e15).valueOf(), 8.64e15);
shouldBe(Number.isFinite(new Date(8.64e15).getFullYear()), true);

const clipOffsetMinutes = new Date(8.64e15).getTimezoneOffset();
if (clipOffsetMinutes < 0) {
    const pulledBack = new Date(275760, 8, 13, 1).valueOf();
    shouldBe(Number.isNaN(pulledBack), false);
    shouldBe(pulledBack <= 8.64e15, true);
}
