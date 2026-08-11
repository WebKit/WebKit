function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected: ${expected}, actual: ${actual}`);
}

for (var i = 0; i < testLoopCount; i++) {
    // ASCII characters
    shouldBe("hello".codePointAt(0), 104);  // 'h'
    shouldBe("hello".codePointAt(1), 101);  // 'e'
    shouldBe("hello".codePointAt(4), 111);  // 'o'

    // BMP characters (Japanese)
    shouldBe("あいう".codePointAt(0), 0x3042);  // 'あ'
    shouldBe("あいう".codePointAt(1), 0x3044);  // 'い'
    shouldBe("あいう".codePointAt(2), 0x3046);  // 'う'

    // Surrogate pairs (emoji) - codePointAt returns the full code point
    shouldBe("😀".codePointAt(0), 0x1F600);  // full emoji code point
    shouldBe("😀".codePointAt(1), 0xDE00);   // low surrogate (when accessed directly)

    // Surrogate pairs in string context
    shouldBe("😀test".codePointAt(0), 0x1F600);  // emoji
    shouldBe("😀test".codePointAt(1), 0xDE00);   // low surrogate
    shouldBe("😀test".codePointAt(2), 116);      // 't'

    // Mixed characters
    shouldBe("a😀b".codePointAt(0), 97);      // 'a'
    shouldBe("a😀b".codePointAt(1), 0x1F600); // emoji
    shouldBe("a😀b".codePointAt(2), 0xDE00);  // low surrogate
    shouldBe("a😀b".codePointAt(3), 98);      // 'b'

    // Multiple surrogate pairs
    shouldBe("𠮷野家".codePointAt(0), 0x20BB7);  // '𠮷' (surrogate pair)
    shouldBe("𠮷野家".codePointAt(1), 0xDFB7);   // low surrogate
    shouldBe("𠮷野家".codePointAt(2), 0x91CE);   // '野'
    shouldBe("𠮷野家".codePointAt(3), 0x5BB6);   // '家'

    // Out of bounds - should return undefined
    shouldBe("hello".codePointAt(5), undefined);
    shouldBe("hello".codePointAt(100), undefined);
    shouldBe("hello".codePointAt(-1), undefined);

    // Empty string
    shouldBe("".codePointAt(0), undefined);
}
