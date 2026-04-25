function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(JSON.stringify(-1), `-1`);
shouldBe(JSON.stringify(-2147483648), `-2147483648`); // INT32_MIN
shouldBe(JSON.stringify(2147483647), `2147483647`); // INT32_MAX
shouldBe(JSON.stringify(0), `0`);
shouldBe(JSON.stringify(1), `1`);
