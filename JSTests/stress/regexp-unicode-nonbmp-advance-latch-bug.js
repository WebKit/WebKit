// When a match attempt fails at a start position, /u patterns advance the start position past a whole
// surrogate pair if and only if the character at the match start is a complete non-BMP surrogate pair.
// Character reads elsewhere in the pattern must not affect this. This test should pass with both
// --useRegExpJIT=true and --useRegExpJIT=false.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(msg + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function shouldBeMatch(re, input, expected) {
    let result = re.exec(input);
    let actual = result === null ? null : [result.index, [...result]];
    shouldBe(JSON.stringify(actual), JSON.stringify(expected), re + " on " + JSON.stringify(input));
}

// A non-BMP character read after the match start must not corrupt the advance amount.
shouldBeMatch(/.x/u, "a\u{10000}x", [1, ["\u{10000}x"]]);
shouldBeMatch(/[^z]x/u, "a\u{10000}x", [1, ["\u{10000}x"]]);
shouldBeMatch(/(.)x/u, "a\u{10000}x", [1, ["\u{10000}x", "\u{10000}"]]);
shouldBeMatch(/.{2}x/u, "ab\u{10000}x", [1, ["b\u{10000}x"]]);
shouldBeMatch(/a|.x/u, "b\u{10000}x", [1, ["\u{10000}x"]]);
shouldBeMatch(/.x/u, "\u{10002}\u{10001}a\u{10000}x", [5, ["\u{10000}x"]]);
shouldBeMatch(/.x/u, "\u{10000}abx", [3, ["bx"]]);
shouldBeMatch(/.x/u, "a\u{10000}y", null);
shouldBeMatch(/.x/u, "\u{10000}", null);

// A lone lead or trail surrogate at the match start advances by only one code unit.
shouldBeMatch(/abc/u, "\uD800abc", [1, ["abc"]]);
shouldBeMatch(/abc/iu, "\uD800ABC", [1, ["ABC"]]);
shouldBeMatch(/q/u, "\uDC00\uD800q", [2, ["q"]]);
shouldBeMatch(/x/u, "\uD800\u{10000}x", [3, ["x"]]);
shouldBeMatch(/\uDC00/u, "\u{10000}\uDC00", [2, ["\uDC00"]]);
shouldBeMatch(/\uD800(?!\uDC00)/u, "\u{10000}\uD800", [2, ["\uD800"]]);

// When longer alternatives no longer fit, a shorter alternative is entered directly at that start position.
shouldBeMatch(/\u{10400}X|Y/u, "\u{10400}ZY", [3, ["Y"]]);
shouldBeMatch(/\u{1F600}ab|c/u, "\u{1F600}xc", [3, ["c"]]);
shouldBeMatch(/\u{1F600}\u{1F600}xy|\u{1F600}z|q/u, "\u{1F600}\u{1F600}q", [4, ["q"]]);
shouldBeMatch(/[a\u{10000}]{3}Z|b/u, "a\u{10000}Xb", [4, ["b"]]);

// A zero minimum-size alternative can match in the middle of a surrogate pair, but the position in
// between is never offered to it: the advance steps over the pair as a whole.
shouldBeMatch(/aa|\B/u, "X\u{1F600}Z", null);
shouldBeMatch(/a?\B|q/u, "X\u{1F600}Z", null);
shouldBeMatch(/[\u{1F600}]X|\B/u, "X\u{1F600}Z", null);
shouldBeMatch(/\B|[\u{1F600}]X/u, "X\u{1F600}Z", null);
shouldBeMatch(/aa|(?=Z)/u, "X\u{1F600}Z", [3, [""]]);
shouldBeMatch(/x+|q/u, "\u{1F600}\u{1F600}q", [4, ["q"]]);

// Start-position prefilters (Boyer-Moore lookahead, alternation SIMD search, shared-lead surrogate scan)
// settle on a candidate position before the advance amount is decided.
shouldBeMatch(/jsc/u, "\u{1F600}\u{1F601}\u{1F602}jsc", [6, ["jsc"]]);
shouldBeMatch(/xyz|abd/u, "\u{1F600}\u{1F600}abd", [4, ["abd"]]);
shouldBeMatch(/\u{1F600}X|\u{1F603}Y/u, "\u{1F603}Q\u{1F603}Y", [3, ["\u{1F603}Y"]]);
shouldBeMatch(/[\u{1F600}-\u{1F64F}]{2}Z|q/u, "\u{1F600}\u{1F601}Wq", [5, ["q"]]);

// Global matching advances lastIndex over surrogate pairs consistently with the above.
{
    let re = /.x/gu;
    let input = "ax\u{10000}x\u{10001}x";
    shouldBe(JSON.stringify(input.match(re)), JSON.stringify(["ax", "\u{10000}x", "\u{10001}x"]), "global .x");

    re = /\B/gu;
    input = "X\u{1F600}Z";
    shouldBe(JSON.stringify(input.match(re)), JSON.stringify(null), "global \\B");

    // A lastIndex the caller assigns is used as given, pair interior or not.
    re.lastIndex = 2;
    let m = re.exec(input);
    shouldBe(m === null ? "null" : m.index + "," + re.lastIndex, "2,2", "global \\B from an assigned lastIndex");
}

// /v (unicodeSets) shares the /u semantics.
shouldBeMatch(new RegExp(".x", "v"), "a\u{10000}x", [1, ["\u{10000}x"]]);
