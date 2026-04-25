function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe("Hello World".indexOf(""), 0);
shouldBe("Hello".indexOf("Hello World"), -1);
shouldBe(("Hello World".repeat(10000) + "Hello World2").indexOf("Hello World2"), 110000);
shouldBe(("Hello World".repeat(10000) + "Hello World2".repeat(10000)).indexOf("Hello World2".repeat(10000)), 110000);

shouldBe("こんにちわ世界".indexOf(""), 0);
shouldBe("こんにちわ".indexOf("こんにちわ世界"), -1);
shouldBe(("こんにちわ世界".repeat(10000) + "Hello World2").indexOf("Hello World2"), 70000);
shouldBe(("こんにちわ世界".repeat(10000) + "Hello World2".repeat(10000)).indexOf("Hello World2".repeat(10000)), 70000);
