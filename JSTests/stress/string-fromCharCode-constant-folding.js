function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected: ${expected}, actual: ${actual}`);
}

for (var i = 0; i < testLoopCount; i++) {
    // ASCII characters
    shouldBe(String.fromCharCode(65), "A");
    shouldBe(String.fromCharCode(97), "a");
    shouldBe(String.fromCharCode(48), "0");
    shouldBe(String.fromCharCode(32), " ");
    shouldBe(String.fromCharCode(10), "\n");

    // Boundary values
    shouldBe(String.fromCharCode(0), "\u0000");
    shouldBe(String.fromCharCode(255), "\u00FF");

    // Latin1 characters
    shouldBe(String.fromCharCode(0xE9), "\u00E9");
    shouldBe(String.fromCharCode(0xF1), "\u00F1");

    // Above maxSingleCharacterString (not folded, but must work correctly)
    shouldBe(String.fromCharCode(256), "\u0100");
    shouldBe(String.fromCharCode(0x3042), "\u3042");
    shouldBe(String.fromCharCode(0xFFFF), "\uFFFF");
}
