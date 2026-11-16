function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var collator = new Intl.Collator('en-US');

shouldBe(collator.compare("ABC", "ABD Ã"), -1);
shouldBe(collator.compare("ABD Ã", "AbE"), -1);
shouldBe(collator.compare("AbE", "ABC"), 1);
shouldBe(collator.compare("ABC", "abc"), 1);
shouldBe(collator.compare("ABC", "ABC"), 0);
shouldBe(collator.compare("abc", "abc"), 0);
shouldBe(collator.compare("abc", "abC"), -1);

shouldBe(collator.compare("AB - AS Foobar", "AB - AS Pulheim KÃƒÂ¤ther"), -1);
shouldBe(collator.compare("AB - AS Pulheim KÃƒÂ¤ther", "Abz - Baz Qux"), -1);
shouldBe(collator.compare("Abz - Baz Qux", "AB - AS Foobar"), 1);
