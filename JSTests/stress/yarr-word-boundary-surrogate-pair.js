//@ runDefault("--useRegExpJIT=false")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${String(expected)} but got: ${String(actual)}`);
}

// \B (non-word-boundary) followed by dot should match a supplementary character
// at the start of the string because the boundary between two non-word characters
// is a non-word-boundary.
{
    let result = /\B./u.exec("\u{10000}\u{10000}");
    shouldBe(result !== null, true);
    shouldBe(result[0], "\u{10000}");
    shouldBe(result.index, 0);
}

// \b (word-boundary) should not match between two supplementary non-word characters.
{
    let result = /\b./u.exec("\u{10000}\u{10000}");
    shouldBe(result, null);
}

// \B with supplementary characters mixed with ASCII word characters.
{
    let result = /\B./u.exec("a\u{10000}");
    shouldBe(result, null);
}

// \b at the start before a supplementary non-word character should not match.
{
    let result = /\b/u.exec("\u{10000}");
    shouldBe(result, null);
}

// \B at the start before a supplementary non-word character should match.
{
    let result = /\B/u.exec("\u{10000}");
    shouldBe(result !== null, true);
    shouldBe(result.index, 0);
}
