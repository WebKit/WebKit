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

// 16-bit ignoreCase backreference — non-Unicode mode
// These exercise the 16-bit ignoreCase backreference loop where
// errorCodePoint checks are dead when !m_decodeSurrogatePairs.
for (let i = 0; i < testLoopCount; i++) {
    // Force Char16 strings by including a character above 0xFF.
    shouldBeArray(/(abc)\1/i.exec("abc\u0100abcABC"), ["abcABC", "abc"], "16-bit ignoreCase backref");
    shouldBe(/(abc)\1/i.exec("abc\u0100abcDEF"), null, "16-bit ignoreCase backref mismatch");

    shouldBeArray(/(Hello)\1/i.exec("\u0100HelloHELLO"), ["HelloHELLO", "Hello"], "16-bit ignoreCase Hello");
    shouldBe(/(Hello)\1/i.exec("\u0100HelloWorld"), null, "16-bit ignoreCase Hello mismatch");

    shouldBeArray(/(ab)\1{3}/i.exec("\u0100ababABAb"), ["ababABAb", "ab"], "16-bit ignoreCase quantified backref");
    shouldBe(/(ab)\1{3}/i.exec("\u0100ababAcd"), null, "16-bit ignoreCase quantified backref mismatch");

    // Lone surrogates in non-Unicode mode — should be treated as plain code units.
    let highSurrogate = "\uD800";
    let input1 = highSurrogate + "x" + highSurrogate + "X";
    shouldBeArray(/(..)\1/i.exec(input1), [highSurrogate + "x" + highSurrogate + "X", highSurrogate + "x"], "16-bit ignoreCase lone high surrogate backref");

    let lowSurrogate = "\uDC00";
    let input2 = lowSurrogate + "a" + lowSurrogate + "A";
    shouldBeArray(/(..)\1/i.exec(input2), [lowSurrogate + "a" + lowSurrogate + "A", lowSurrogate + "a"], "16-bit ignoreCase lone low surrogate backref");
}

// 16-bit ignoreCase backreference — Unicode mode
// These exercise the same loop but with m_decodeSurrogatePairs=true,
// ensuring errorCodePoint checks are still emitted when needed.
for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/(abc)\1/iu.exec("abc\u0100abcABC"), ["abcABC", "abc"], "unicode 16-bit ignoreCase backref");
    shouldBe(/(abc)\1/iu.exec("abc\u0100abcDEF"), null, "unicode 16-bit ignoreCase backref mismatch");

    shouldBeArray(/(\u{1F600})\1/u.exec("\u{1F600}\u{1F600}"), ["\u{1F600}\u{1F600}", "\u{1F600}"], "unicode non-BMP backref");
    shouldBe(/(\u{1F600})\1/u.exec("\u{1F600}\u{1F601}"), null, "unicode non-BMP backref mismatch");

    shouldBeArray(/(abc)\1/iu.exec("\u{10000}abcABC"), ["abcABC", "abc"], "unicode 16-bit ignoreCase after non-BMP");
}
