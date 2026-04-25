function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(String.prototype.toWellFormed.call(1), '1');
shouldBe(String.prototype.toWellFormed.call(42.5), '42.5');
