function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(eval("'\u2028Japanese'").charCodeAt(0), 0x2028);
shouldBe(eval("'\u2029Japanese'").charCodeAt(0), 0x2029);
shouldBe(eval("'\u2028日本語'").charCodeAt(0), 0x2028);
shouldBe(eval("'\u2029日本語'").charCodeAt(0), 0x2029);

shouldBe(eval("'\u2028Japanese' + 'hello' + 'world'").charCodeAt(0), 0x2028);
shouldBe(eval("'\u2029Japanese' + 'hello' + 'world'").charCodeAt(0), 0x2029);
shouldBe(eval("'\u2028日本語' + 'hello' + 'world'").charCodeAt(0), 0x2028);
shouldBe(eval("'\u2029日本語' + 'hello' + 'world'").charCodeAt(0), 0x2029);
