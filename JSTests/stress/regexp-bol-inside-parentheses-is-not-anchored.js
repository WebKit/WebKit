// A `^` inside a parenthesis only anchors the pattern when that parenthesis has to be matched. Getting
// this wrong made optimizeBOL() treat the alternative as once-through, only ever tried at index 0.
// See recomputeStartsWithBOL() in YarrPattern.cpp. The bug is in Yarr's pattern analysis, so it
// reproduces in every tier; the loop only makes sure the JIT tiers agree.

function shouldBe(actual, expected, context) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected + " for " + context);
}

function check(re, string, expectedTest, expectedMatch, expectedIndex) {
    const context = re + " against " + JSON.stringify(string);
    shouldBe(re.test(string), expectedTest, context);
    const result = re.exec(string);
    if (expectedMatch === null) {
        shouldBe(result, null, context);
        return;
    }
    shouldBe(result[0], expectedMatch, context);
    shouldBe(result.index, expectedIndex, context);
}

function step() {
    // A leading parenthesis that can match empty leaves the pattern unanchored.
    check(/(?:^a)?b/, "zzb", true, "b", 2);
    check(/(?:^a)*b/, "zzb", true, "b", 2);
    check(/(?:^a){0,1}b/, "zzb", true, "b", 2);
    check(/(?:(?:^a)?)b/, "zzb", true, "b", 2);
    check(/(?:^a)?[bc]/, "zzc", true, "c", 2);
    check(/(?:^[^\x00-\xfe])?b/, "zzb", true, "b", 2);
    check(/(?:^a)?b|q/, "zzb", true, "b", 2);
    check(/((?:^a)?)b/, "zzb", true, "b", 2);
    check(/(?:^a|^c)?b/, "zzb", true, "b", 2);

    // The anchored spelling must still match at 0, and an unmatchable subject must still be a no-match.
    check(/(?:^a)?b/, "ab", true, "ab", 0);
    check(/(?:^a)?b/, "zzz", false, null);

    // The parenthesis need not be the leading term.
    check(/x(?:^a)?b/, "zxb", true, "xb", 1);
    // ... nor at the top level: the once-through/loop split filters at every nesting depth.
    check(/x(?:(?:(?:^a)?q|b)c)y/, "zxqcy", true, "xqcy", 1);
    check(/x(?:(?:(?:^a)?q|b)c)y/, "zxbcy", true, "xbcy", 1);
    check(/x(?:^a)?b/, "xb", true, "xb", 0);

    // A parenthesis that must be matched and can only match at 0 makes the whole alternative
    // impossible elsewhere, so the loop alternative has to be dropped, not just the parenthesis.
    check(/x(?:^a)b/, "xb", false, null);
    check(/x(?:^a)b/, "xab", false, null);
    check(/x(?:^a)b/, "zxab", false, null);
    check(/x(?=^a)b/, "xb", false, null);
    check(/x(?=^a)ab/, "xab", false, null);

    // A negative assertion whose content can only match at 0 succeeds everywhere else, so its term
    // can be dropped from the loop alternative.
    check(/(?!^a)b/, "zzb", true, "b", 2);
    check(/(?!^a)b/, "ab", true, "b", 1);
    check(/(?!^a)a/, "ab", false, null);

    // Genuinely anchored patterns must keep matching only at 0.
    check(/(?:^a)b/, "ab", true, "ab", 0);
    check(/(?:^a)b/, "zab", false, null);
    check(/^ab/, "ab", true, "ab", 0);
    check(/^ab/, "zab", false, null);
    check(/(?:^a|^b)c/, "bc", true, "bc", 0);
    check(/(?:^a|^b)c/, "zbc", false, null);

    // With the m flag optimizeBOL() bails out entirely; that must not regress either.
    check(/(?:^a)?b/m, "zzb", true, "b", 2);
    check(/(?:^a)?b/m, "z\nab", true, "ab", 2);
}

for (var i = 0; i < testLoopCount; ++i)
    step();
