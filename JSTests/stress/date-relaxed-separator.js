function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(new Date("2022-01-01 00:00:00Z").getTime(), 1640995200000);
shouldBe(new Date("2022-01-01t00:00:00Z").getTime(), 1640995200000);
