// A pattern can look ^-anchored without every match beginning at index 0, and the first-character
// fast-fail filter (DFG/FTL, position 0) must reject those on its own rather than trusting
// PatternAlternative::m_startsWithBOL, which is a Yarr optimization hint maintained far away from
// here. The case that matters: optimizeDotStarWrappedExpressions() rewrites /^.*X.*$/ by DELETING
// the leading ^ and .* and appending a DotStarEnclosure, so the first surviving term is no longer
// where the match begins.
//
// Every case must produce the same answer in every tier, so each runs in a loop long enough to tier
// up. Cases that must NOT match are included so a "never filter anything" regression is caught too.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function check(re, string, expectedTest, expectedMatch, expectedIndex) {
    re.lastIndex = 0;
    shouldBe(re.test(string), expectedTest);
    re.lastIndex = 0;
    const result = re.exec(string);
    if (expectedMatch === null) {
        shouldBe(result, null);
        return;
    }
    shouldBe(result[0], expectedMatch);
    shouldBe(result.index, expectedIndex);
}

// Dot-star wrapped expressions: the leading .* means the match starts at 0 even though the first
// surviving term appears later in the subject, so no bitmap built from that term may be applied.
function dotStarWrapped() {
    check(/^.*[^x].*$/, "xa", true, "xa", 0);
    check(/^.*foo.*$/, "xfoo", true, "xfoo", 0);
    check(/^.*foo.*/, "xfoo", true, "xfoo", 0);
    check(/^.*[abc].*$/, "xa", true, "xa", 0);
    check(/^.*\D.*$/, "1a", true, "1a", 0);
    check(/^.*\W.*$/, "a!", true, "a!", 0);
    check(/^.*[^0-9].*$/, "9a", true, "9a", 0);
    check(/^.*(?:[^q]).*$/, "qa", true, "qa", 0);
    check(/^.*foo.*$/, "xbar", false, null);

    // The rewrite needs both a leading and a trailing .*; without the trailing one the leading .*
    // survives as a term and folding it into the bitmap is sound.
    check(/^.*foo/, "xfoo", true, "xfoo", 0);
    check(/^.*foo/, "xbar", false, null);

    // A capturing term blocks the rewrite entirely.
    check(/^.*(x).*$/, "yx", true, "yx", 0);

    // Multiline is rejected before the filter is even considered.
    check(/^.*foo.*$/m, "x\nfoo", true, "foo", 2);
}

// A genuinely anchored pattern must still be filtered, and must still be right.
function genuinelyAnchored() {
    check(/^foo/, "foobar", true, "foo", 0);
    check(/^foo/, "xfoobar", false, null);
    check(/^[^x]/, "ya", true, "y", 0);
    check(/^[^x]/, "xa", false, null);
    check(/^(?:a|b)c/, "bc", true, "bc", 0);
    check(/^(?:a|b)c/, "zbc", false, null);
    check(/^(?:^a)b/, "ab", true, "ab", 0);
    check(/^(?:^a)b/, "zab", false, null);
}

for (var i = 0; i < testLoopCount; ++i) {
    dotStarWrapped();
    genuinelyAnchored();
}
