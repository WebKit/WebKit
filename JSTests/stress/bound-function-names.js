function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() { }
shouldBe(test.bind().name, "bound test");
shouldBe(test.bind().bind().name, "bound bound test");
shouldBe(test.bind().bind().bind().name, "bound bound bound test");
