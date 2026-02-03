function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
shouldBe(JSON.parse("999999999999999"), 999999999999999);
shouldBe(JSON.parse("9999999999999999"), 9999999999999999);
shouldBe(JSON.parse("-999999999999999"), -999999999999999);
shouldBe(JSON.parse("-9999999999999999"), -9999999999999999);
