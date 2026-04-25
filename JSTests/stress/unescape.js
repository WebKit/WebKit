function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(unescape("%0"), "%0");
shouldBe(unescape("%a"), "%a");
