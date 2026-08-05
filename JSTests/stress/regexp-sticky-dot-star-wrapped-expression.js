// optimizeDotStarWrappedExpressions() rewrites /^.*EXPR.*$/ into EXPR plus a DotStarEnclosure term
// that stands in for the deleted `^` and the two `.*`. The enclosure reports the position of EXPR
// rather than the position the leading `.*` starts at, which is fine for an ordinary search but not
// for a sticky pattern: a sticky match must begin exactly at lastIndex, so /^.*a.*$/y on "xa" was
// rejected instead of matching the whole string at index 0.
//
// This is a Yarr-level bug, so it reproduces in every tier including the interpreter; the loops here
// only make sure the JIT tiers agree.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function check(re, lastIndex, string, expectedTest, expectedLastIndexAfterTest, expectedMatch) {
    re.lastIndex = lastIndex;
    shouldBe(re.test(string), expectedTest);
    shouldBe(re.lastIndex, expectedLastIndexAfterTest);

    re.lastIndex = lastIndex;
    const result = re.exec(string);
    if (expectedMatch === null) {
        shouldBe(result, null);
        return;
    }
    shouldBe(result[0], expectedMatch);
    shouldBe(result.index, lastIndex);
}

const sticky = /^.*a.*$/y;
const stickyGlobal = /^.*a.*$/gy;
const stickyInverted = /^.*[^q].*$/y;
const stickyNoEOL = /^.*a.*/y;

function step() {
    // The leading .* consumes "x", so the match begins at lastIndex 0 and covers the whole string.
    check(sticky, 0, "xa", true, 2, "xa");
    check(sticky, 0, "a", true, 1, "a");
    check(sticky, 0, "zzza", true, 4, "zzza");
    check(sticky, 0, "xb", false, 0, null);
    check(sticky, 0, "", false, 0, null);

    // A non-zero lastIndex still cannot match, because ^ only matches at index 0.
    check(sticky, 1, "xa", false, 0, null);
    check(sticky, 2, "xa", false, 0, null);

    check(stickyGlobal, 0, "xa", true, 2, "xa");
    check(stickyGlobal, 1, "xa", false, 0, null);

    check(stickyInverted, 0, "qa", true, 2, "qa");
    check(stickyInverted, 0, "qq", false, 0, null);

    check(stickyNoEOL, 0, "xa", true, 2, "xa");
    check(stickyNoEOL, 0, "xb", false, 0, null);

    // Non-sticky spellings keep using the enclosure and must be unaffected.
    shouldBe(/^.*a.*$/.test("xa"), true);
    shouldBe(/^.*a.*$/.test("xb"), false);
    shouldBe(/^.*a.*$/.exec("zzza")[0], "zzza");
    shouldBe(/^.*a.*$/g.test("xa"), true);
}

for (var i = 0; i < testLoopCount; ++i)
    step();
