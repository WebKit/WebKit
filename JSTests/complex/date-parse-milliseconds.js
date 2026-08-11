function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(Date.parse("2514-06-01T00:00:00.148Z"), 17180035200148);
shouldBe(Date.parse("2514-06-01T00:00:00.14Z"), 17180035200140);
shouldBe(Date.parse("2514-06-01T00:00:00.1Z"), 17180035200100);
