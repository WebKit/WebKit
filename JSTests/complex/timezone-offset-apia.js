function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(new Date("2019-05-01").getTimezoneOffset() / -60, 13);
shouldBe(new Date("2019-11-01").getTimezoneOffset() / -60, 14);
