// DFG StrengthReductionPhase folds StringReplace when the string, regexp, and replacement
// are all constants. For empty matches with /gu or /gv, it must advance past a full surrogate
// pair, not just by 1 UTF-16 code unit. Otherwise the DFG-folded result differs from the
// runtime result.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: actual=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

// U+1D49C (MATHEMATICAL SCRIPT CAPITAL A) = surrogate pair \uD835\uDC9C
// Expected: insertions at positions 0 and 2 (before/after the code point), not at 0,1,2
// Non-unicode: "X\uD835X\uDC9CX" (3 insertions) — spec says unicode flag treats surrogate pair as 1 code point
const expected_gu = "X\uD835\uDC9CX";

function testReplaceGU() {
    return "\uD835\uDC9C".replace(/(?:)/gu, "X");
}
noInline(testReplaceGU);

function testReplaceGV() {
    return "\uD835\uDC9C".replace(/(?:)/gv, "X");
}
noInline(testReplaceGV);

// Also test with a replacement string (non-empty replace path)
function testReplaceGUNonEmpty() {
    return "\uD835\uDC9C".replace(/(?:)/gu, "_");
}
noInline(testReplaceGUNonEmpty);

// Multiple surrogate pairs + BMP chars mixed: "a" + U+1D49C + "b" + U+1D49D
// positions: 0(a) 1(pair) 3(b) 4(pair) 6(end) => 5 insertion points
const expectedMixed = "_a_\uD835\uDC9C_b_\uD835\uDC9D_";
function testMixed() {
    return "a\uD835\uDC9Cb\uD835\uDC9D".replace(/(?:)/gu, "_");
}
noInline(testMixed);

// Lone surrogate (unpaired) — should advance by 1 since not a valid pair
// "\uD835a" — lead without trail => positions 0,1,2 => 3 insertions
const expectedLone = "_\uD835_a_";
function testLoneSurrogate() {
    return "\uD835a".replace(/(?:)/gu, "_");
}
noInline(testLoneSurrogate);

// Compute expected via interpreter (first iteration, before DFG)
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(testReplaceGU(), expected_gu);
    shouldBe(testReplaceGV(), expected_gu);
    shouldBe(testReplaceGUNonEmpty(), "_\uD835\uDC9C_");
    shouldBe(testMixed(), expectedMixed);
    shouldBe(testLoneSurrogate(), expectedLone);
}
