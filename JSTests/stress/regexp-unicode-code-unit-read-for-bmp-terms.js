function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}, expected ${expected}`);
}

function shouldBeMatch(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

for (let i = 0; i < 200; ++i) {
    // BMP-only classes reading a surrogate pair position must not match either half.
    shouldBeMatch(/[a-z]/u.exec("\u{1F600}b"), ["b"]);
    shouldBeMatch(/\w+/u.exec("\u{1F600}ab\u{1F601}"), ["ab"]);
    shouldBeMatch(/[\d\s]/u.exec("\uD83D 1"), [" "]);
    shouldBeMatch(/\w{2}\s/u.exec("ab\uD83Dcd "), ["cd "]);

    // Reading in the middle of a surrogate pair (sticky lastIndex on the trail surrogate).
    let sticky = /[a-z\u{1F600}]/uy;
    sticky.lastIndex = 1;
    shouldBeMatch(sticky.exec("\u{1F600}x"), null);
    sticky = /\w/uy;
    sticky.lastIndex = 1;
    shouldBeMatch(sticky.exec("\u{1F600}x"), null);

    // Lone surrogates are never members of a surrogate-free class.
    shouldBeMatch(/[\s\S]&&[a\uD800]/v.exec("\uD800a"), null);
    shouldBeMatch(/[a-z]|q/u.exec("\uDBFFz\uDC00"), ["z"]);

    // Word boundary around non-BMP characters and lone surrogates.
    shouldBeMatch(/\b\w+\b/u.exec("\u{1F600}abc\u{1F600}"), ["abc"]);
    shouldBeMatch(/\B\w/u.exec("\uD83Dab"), ["b"]);
    shouldBeMatch(/a\b/u.exec("a\u{1F600}"), ["a"]);
    shouldBeMatch(/\bK/iu.exec("K"), ["K"]);
    shouldBeMatch(/\b\w\b/iu.exec("\u{1F600} ſ \u{1F600}"), ["ſ"]);

    // Multiline anchors next to non-BMP characters.
    shouldBeMatch(/^b$/mu.exec("\u{1F600}\nb\n\u{1F601}"), ["b"]);
    shouldBeMatch(/^\w+$/mu.exec("a\u{1F600}\nxyz\n"), ["xyz"]);
    shouldBeMatch(/x$/mu.exec("x "), ["x"]);

    // A BMP pattern character next to a non-BMP class term keeps decoding for the class.
    shouldBeMatch(/a[\u{1F600}-\u{1F64F}]b/u.exec("xa\u{1F60D}b"), ["a\u{1F60D}b"]);
    shouldBeMatch(/a?\B|q/u.exec("X\u{1F600}Z"), [""]);
}
