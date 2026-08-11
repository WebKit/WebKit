// Verify the anchored (^, non-multiline) RegExp.test first-character fast-fail filter: a filtered
// no-match must equal the operation's result, non-anchored patterns must never be wrongly filtered,
// case-insensitive folds must be reflected in the bitmap, and a .compile() that changes the pattern
// must be handled (the recompile watchpoint jettisons the code holding the stale bitmap).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function step() {
    // Anchored ^ .test() on constant regexes (filtered when input[0] can't begin a match).
    shouldBe(/^(?:await|case|return)$/.test("case"), true);
    shouldBe(/^(?:await|case|return)$/.test(";"), false);       // filtered
    shouldBe(/^(?:await|case|return)$/.test("config"), false);  // 'c' passes filter, op fails
    shouldBe(/^[0-9]+$/.test("42"), true);
    shouldBe(/^[0-9]+$/.test("x4"), false);                     // filtered
    shouldBe(/^#\w+/.test("#tag"), true);
    shouldBe(/^#\w+/.test("tag"), false);                       // filtered

    // Case-insensitive patterns ARE filtered (the bitmap folds ASCII case; Latin-1 letters with a
    // case pair are folded character classes); results must be correct across the folded set.
    shouldBe(/^hsl/i.test("HSL(1,2%,3%)"), true);
    shouldBe(/^hsl/i.test("hsl(1)"), true);
    shouldBe(/^hsl/i.test("rgb"), false);
    shouldBe(/^[a-f]+$/i.test("ABC"), true);
    shouldBe(/^ä/i.test("Ä!"), true);   // Latin-1 letter with case pair (folded class)
    shouldBe(/^ä/i.test("x"), false);
    shouldBe(/^k/i.test("K"), true);

    // Characters whose Unicode case-fold partner is ASCII (U+212A KELVIN -> k/K, U+017F LONG S ->
    // s/S) become folded character classes under /iu; the ASCII partner must be in the bitmap so the
    // filter does not wrongly fast-fail an ASCII input. Under non-unicode /i they do NOT fold to
    // ASCII (they only match themselves).
    shouldBe(/^K/iu.test("k"), true);
    shouldBe(/^K/iu.test("K"), true);
    shouldBe(/^K/iu.test("x"), false);
    shouldBe(/^ſ/iu.test("s"), true);
    shouldBe(/^ſ/iu.test("q"), false);
    shouldBe(/^K/i.test("k"), false);        // non-unicode: KELVIN does not fold to ASCII
    shouldBe(/^K/i.test("K"), true);    // but matches itself

    // Non-anchored .test(): match can be anywhere, never filtered.
    shouldBe(/foo/.test("xxfoo"), true);
    shouldBe(/[0-9]/.test("abc9"), true);

    // Multiline ^ is not anchored-at-0.
    shouldBe(/^bar/m.test("x\nbar"), true);
}

for (var i = 0; i < testLoopCount; ++i)
    step();

// A .compile() that changes the pattern fires the realm's RegExp-recompiled watchpoint, jettisoning
// the code that baked the old bitmap: the old filter must not produce wrong answers for the new
// pattern.
(function () {
    var re = /^a+/;
    function check(a, b) { shouldBe(re.test(a), b); }
    for (var i = 0; i < testLoopCount; ++i) { check("abc", true); check("zzz", false); }
    re.compile("^z+");
    for (var i = 0; i < testLoopCount; ++i) { check("zzz", true); check("abc", false); }
})();
