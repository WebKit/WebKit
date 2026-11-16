function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function shouldBeArray(actual, expected, msg) {
    if (actual === null && expected !== null)
        throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got null`);
    if (actual === null && expected === null)
        return;
    if (actual.length !== expected.length)
        throw new Error(`FAIL: ${msg}: length mismatch: expected ${expected.length}, got ${actual.length}`);
    for (let i = 0; i < expected.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error(`FAIL: ${msg}: index ${i}: expected ${JSON.stringify(expected[i])}, got ${JSON.stringify(actual[i])}`);
    }
}

for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/(abc)\1/.exec("abcabc"), ["abcabc", "abc"], "basic backref match");
    shouldBe(/(abc)\1/.exec("abcdef"), null, "basic backref mismatch");
    shouldBe(/(abcd)\1/.exec("abcdabce"), null, "partial backref mismatch");

    shouldBeArray(/(\u00e9)\1/u.exec("\u00e9\u00e9"), ["\u00e9\u00e9", "\u00e9"], "unicode BMP backref");
    shouldBeArray(/(.)\1/u.exec("\u{1F600}\u{1F600}"), ["\u{1F600}\u{1F600}", "\u{1F600}"], "non-BMP backref match");
    shouldBe(/(.)\1/u.exec("\u{1F600}\u{1F601}"), null, "non-BMP backref mismatch");
    shouldBeArray(/(.\u0041)\1/u.exec("\u{10000}A\u{10000}A"), ["\u{10000}A\u{10000}A", "\u{10000}A"], "astral+BMP backref");

    shouldBeArray(/(a)\1{9}/.exec("aaaaaaaaaa"), ["aaaaaaaaaa", "a"], "quantified backref x10");
    shouldBe(/(a)\1{9}/.exec("aaaaaaaab"), null, "quantified backref mismatch at end");

    shouldBeArray(/(abc)\1/i.exec("abcABC"), ["abcABC", "abc"], "case-insensitive backref");
    shouldBe(/(abc)\1/i.exec("abcDEF"), null, "case-insensitive backref mismatch");

    shouldBeArray(/(?:(abc)|(def))\1/.exec("abcabc"), ["abcabc", "abc", undefined], "backref in alternation");
    shouldBeArray(/()\1/.exec("abc"), ["", ""], "empty capture backref");

    shouldBeArray(/(a)(b)\2\1/.exec("abba"), ["abba", "a", "b"], "multiple backrefs");
    shouldBe(/(a)(b)\2\1/.exec("abab"), null, "multiple backrefs mismatch");

    shouldBeArray(/(\u{1F600})\1/u.exec("\u{1F600}\u{1F600}"), ["\u{1F600}\u{1F600}", "\u{1F600}"], "unicode emoji backref");

    shouldBe(/(abc)\1/.test("abcabc"), true, "test() backref match");
    shouldBe(/(abc)\1/.test("abcdef"), false, "test() backref mismatch");
}

{
    let highSurrogate = "\uD800";
    let input = highSurrogate + "x" + highSurrogate + "x";
    for (let i = 0; i < testLoopCount; i++)
        shouldBeArray(/(..)\1/.exec(input), [input, highSurrogate + "x"], "lone high surrogate backref");
}

{
    let lowSurrogate = "\uDC00";
    let input = lowSurrogate + "y" + lowSurrogate + "y";
    for (let i = 0; i < testLoopCount; i++)
        shouldBeArray(/(..)\1/.exec(input), [input, lowSurrogate + "y"], "lone low surrogate backref");
}

{
    let chunk = "abcdefghij";
    let input = chunk + chunk;
    for (let i = 0; i < testLoopCount; i++)
        shouldBeArray(new RegExp("(" + chunk + ")\\1").exec(input), [input, chunk], "long repeated chunk backref");
}
