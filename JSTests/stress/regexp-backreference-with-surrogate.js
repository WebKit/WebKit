function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

// A backreference reading a non-BMP character must not corrupt
// firstCharacterAdditionalReadSize and skip the next match start position.
shouldBe(/(.)\1/u.exec("a\u{10000}\u{10000}b").index, 1);
shouldBe(/(.)\1/u.test("ab\u{10000}\u{10000}c"), true);
shouldBe(/(.)\1/iu.exec("a\u{10000}\u{10000}b").index, 1);
shouldBe("a\u{10000}\u{10000}b".replace(/(.)\1/u, "X"), "aXb");
shouldBe(/(.)\1/u.exec("\u{10000}a\u{10000}\u{10000}").index, 3);
