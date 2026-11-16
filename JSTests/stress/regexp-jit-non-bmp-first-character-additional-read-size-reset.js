/*
 * Regression test for non-BMP first-character optimization stale register.
 *
 * BodyAlternativeNext / once-through-End reentry labels did not reset
 * m_regs.firstCharacterAdditionalReadSize. After a prior alternative read a
 * surrogate-pair character (setting the register to 1), the body loop would
 * incorrectly advance the index by an extra unit, skipping valid match
 * positions in subsequent alternatives.
 *
 * Triggered when m_useFirstNonBMPCharacterOptimization is enabled
 * (decodeSurrogatePairs && !inlineTest && !multiline && !containsBOL
 * && !containsLookbehinds && !containsModifiers).
 */

function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: actual=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

for (var i = 0; i < 1e4; ++i) {
    // Two alternatives: alt1 reads non-BMP, alt2 should match later position.
    shouldBe("\u{10400}ZY".match(/\u{10400}X|Y/u), ["Y"]);
    shouldBe("\u{1F600}Xc".match(/\u{1F600}b|c/u), ["c"]);

    // Same shape with leading prefix forcing alt1 to consume non-BMP first.
    shouldBe("a\u{1F600}X".match(/a\u{1F600}b|\u{1F600}/u), ["\u{1F600}"]);

    // CharacterClass containing a non-BMP code point with a quantifier.
    shouldBe("a\u{1F600}X".match(/a[\u{1F600}]+b|\u{1F600}/u), ["\u{1F600}"]);

    // Mixed-width character class repeated, then second alternative.
    shouldBe("\u{10400}\u{10400}ZY".match(/[\u{10400}a]{2}X|Y/u), ["Y"]);

    // leftmost-first invariant: when both alternatives can match, the
    // earliest position must win. Bug version returned the longer alt at
    // a later index because the loop over-advanced.
    var m = "a\u{1F600}Xa\u{1F600}b".match(/a\u{1F600}b|\u{1F600}/u);
    shouldBe(m[0], "\u{1F600}");
    shouldBe(m.index, 1);

    // Sanity: with /m the optimization is disabled, behaviour must match.
    shouldBe("\u{10400}ZY".match(/\u{10400}X|Y/um), ["Y"]);

    // Body loop iterates: prior alt reads non-BMP, body advances by one,
    // alt2 must find the match at the next code unit.
    shouldBe("ZZ\u{1F600}aXY".match(/\u{1F600}aZ|XY/u), ["XY"]);
}
