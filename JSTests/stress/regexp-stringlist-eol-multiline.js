function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

// Alternation must prefer the earlier alternative "a", which matches because
// multiline $ succeeds before the line terminator.
shouldBe(/^(?:a|a\nx)$/m.exec("a\nx")[0], "a");
shouldBe(/^(?:a|a\nx)$/m.test("a\nx"), true);

// "a\nb" matches the text but fails $ (next char is "Q"); the later
// alternative "a" must then be tried and succeeds before the "\n".
shouldBe(/^(?:a\nb|a)$/m.exec("a\nbQ")[0], "a");

shouldBe(/^(?:foo|bar|f)$/m.exec("foo")[0], "foo");
shouldBe(/^(?:foo|bar|f)$/m.exec("bar\nzzz")[0], "bar");
shouldBe(/^(?:foo|bar|f)$/m.exec("f\nbar")[0], "f");
shouldBe(/^(?:foo|bar|f)$/m.exec("fo"), null);
