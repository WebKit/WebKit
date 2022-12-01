//@ requireOptions("--useStringWellFormed=1")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe("".isWellFormed(), true);
shouldBe("".toWellFormed(), "");

shouldBe("Hello World".isWellFormed(), true);
shouldBe("Hello World".toWellFormed(), "Hello World");

shouldBe("こんにちわ".isWellFormed(), true);
shouldBe("こんにちわ".toWellFormed(), "こんにちわ");

shouldBe("𠮷野家".isWellFormed(), true);
shouldBe("𠮷野家".toWellFormed(), "𠮷野家");

shouldBe("A\uD842".isWellFormed(), false);
shouldBe("A\uD842".toWellFormed(), "A\uFFFD");

shouldBe("A\uD842A".isWellFormed(), false);
shouldBe("A\uD842A".toWellFormed(), "A\uFFFDA");

shouldBe("A\uD842\uDFB7".isWellFormed(), true);
shouldBe("A\uD842\uDFB7".toWellFormed(), "A\uD842\uDFB7");

shouldBe("A\uDFB7".isWellFormed(), false);
shouldBe("A\uDFB7".toWellFormed(), "A\uFFFD");

shouldBe("A\uDFB7\uD842".isWellFormed(), false);
shouldBe("A\uDFB7\uD842".toWellFormed(), "A\uFFFD\uFFFD");
