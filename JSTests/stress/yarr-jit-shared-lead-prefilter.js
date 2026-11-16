// Tests for the YarrJIT BodyAlternative shared-lead-surrogate prefilter and the
// per-term shared-lead trail-only fast path. These optimizations only fire under
// /u or /v on Char16 input; the tests exercise both the positive paths
// (alternations whose first consuming term shares a UTF-16 lead surrogate) and
// the negatives that must NOT trigger the prefilter.

function shouldBe(actual, expected, message)
{
    if (actual !== expected)
        throw new Error(message + ": expected " + String(expected) + " but got " + String(actual));
}

function shouldBeArray(actual, expected, message)
{
    if (actual === null && expected === null)
        return;
    if (actual === null || expected === null)
        throw new Error(message + ": one of actual/expected is null (actual=" + String(actual) + ", expected=" + String(expected) + ")");
    if (actual.length !== expected.length)
        throw new Error(message + ": length mismatch — expected " + expected.length + " got " + actual.length);
    for (let i = 0; i < expected.length; ++i) {
        if (actual[i] !== expected[i])
            throw new Error(message + "[" + i + "]: expected " + String(expected[i]) + " got " + String(actual[i]));
    }
}

// Run a regex twice — once on a fresh literal, once on a String constructor
// result — so both Char8/Char16 representations of the haystack are exercised.
function execBoth(reSource, flags, input, expectedMatch)
{
    let re1 = new RegExp(reSource, flags);
    let m1 = re1.exec(input);
    if (expectedMatch === null)
        shouldBe(m1, null, "exec(" + JSON.stringify(input) + ") via RegExp(" + JSON.stringify(reSource) + "," + JSON.stringify(flags) + ")");
    else {
        shouldBe(m1 !== null, true, "exec must match: " + JSON.stringify(reSource) + " on " + JSON.stringify(input));
        shouldBe(m1[0], expectedMatch, "exec[0]: " + JSON.stringify(reSource) + " on " + JSON.stringify(input));
    }

    let re2 = new RegExp(reSource, flags);
    let m2 = re2.test(input);
    shouldBe(m2, expectedMatch !== null, "test(): " + JSON.stringify(reSource) + " on " + JSON.stringify(input));
}

// ----------------------------------------------------------------------------
// 1. PatternCharacter alternation, all sharing UTF-16 lead 0xD83C.
//    This exercises the new findFirstTermSharedLead PatternCharacter branch.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|\u{1F0B1}|\u{1F0C1}|\u{1F0D1}/u;

    shouldBe(re.test("\u{1F0A1}"), true, "literal-alt: AS");
    shouldBe(re.test("\u{1F0B1}"), true, "literal-alt: HS");
    shouldBe(re.test("\u{1F0C1}"), true, "literal-alt: DS");
    shouldBe(re.test("\u{1F0D1}"), true, "literal-alt: CS");

    // Same lead, codepoint not in the alternation — must reject.
    shouldBe(re.test("\u{1F0A2}"), false, "literal-alt: same lead but not enumerated");
    shouldBe(re.test("\u{1F0DE}"), false, "literal-alt: end-of-card-range, not enumerated");

    // Different lead surrogate — must reject without misreading the pair.
    shouldBe(re.test("\u{1F1E6}"), false, "literal-alt: different lead 0xD83C->0xD83C, low pair differs");
    shouldBe(re.test("\u{20000}"), false, "literal-alt: different lead 0xD840");

    // BMP haystack — no surrogate pair at all.
    shouldBe(re.test("ABCD"), false, "literal-alt: BMP haystack");
    shouldBe(re.test(""), false, "literal-alt: empty haystack");

    // Lone surrogate — must not match (no pair to decode).
    shouldBe(re.test("\uD83C"), false, "literal-alt: lone lead surrogate");
    shouldBe(re.test("\uDCA1"), false, "literal-alt: lone trail surrogate");

    // Match anywhere in the string (non-anchored).
    shouldBeArray(re.exec("foo\u{1F0B1}bar"), ["\u{1F0B1}"], "literal-alt: middle of string");
    shouldBeArray(re.exec("\u{1F600}\u{1F0C1}"), ["\u{1F0C1}"], "literal-alt: after another non-BMP char");
}

// ----------------------------------------------------------------------------
// 2. PatternCharacter alternation, mixed lead surrogates.
//    Prefilter must NOT fire (or must fire with no shared lead) but the regex
//    must still match correctly.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|\u{1F1E6}|\u{20000}/u;
    shouldBe(re.test("\u{1F0A1}"), true, "mixed-lead: 1F0A1");
    shouldBe(re.test("\u{1F1E6}"), true, "mixed-lead: 1F1E6");
    shouldBe(re.test("\u{20000}"), true, "mixed-lead: 20000");
    shouldBe(re.test("\u{1F0A2}"), false, "mixed-lead: not in set");
    shouldBe(re.test("ABC"), false, "mixed-lead: BMP haystack");
}

// ----------------------------------------------------------------------------
// 3. CharacterClass shared-lead — the original UniPoker shape.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}-\u{1F0DE}]/u;
    for (let cp = 0x1F0A1; cp <= 0x1F0DE; ++cp)
        shouldBe(re.test(String.fromCodePoint(cp)), true, "card-class: " + cp.toString(16));

    shouldBe(re.test("\u{1F0A0}"), false, "card-class: just below");
    shouldBe(re.test("\u{1F0DF}"), false, "card-class: just above");
    shouldBe(re.test("\u{1F600}"), false, "card-class: same lead, different trail");
    shouldBe(re.test("A"), false, "card-class: BMP");
}

// ----------------------------------------------------------------------------
// 4. CharacterClass alternation — multiple classes all sharing a lead.
//    Each first-term is a class with hasSharedLeadSurrogate(); the prefilter
//    should compute a single shared lead across all alternatives.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}\u{1F0A2}]|[\u{1F0B1}\u{1F0B2}]|[\u{1F0C1}\u{1F0C2}]/u;
    shouldBe(re.test("\u{1F0A1}"), true, "class-alt: 1F0A1");
    shouldBe(re.test("\u{1F0B2}"), true, "class-alt: 1F0B2");
    shouldBe(re.test("\u{1F0C1}"), true, "class-alt: 1F0C1");
    shouldBe(re.test("\u{1F0A3}"), false, "class-alt: 1F0A3 (same lead, not enumerated)");
    shouldBe(re.test("\u{1F600}"), false, "class-alt: same lead, different trail");
}

// ----------------------------------------------------------------------------
// 5. Mixed PatternCharacter + CharacterClass alternation, shared lead.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|[\u{1F0B1}-\u{1F0BE}]|\u{1F0C1}/u;
    shouldBe(re.test("\u{1F0A1}"), true, "mixed: literal A");
    shouldBe(re.test("\u{1F0B5}"), true, "mixed: range middle");
    shouldBe(re.test("\u{1F0C1}"), true, "mixed: literal C");
    shouldBe(re.test("\u{1F0BF}"), false, "mixed: just past range");
    shouldBe(re.test("\u{1F0D1}"), false, "mixed: same lead, not enumerated");
}

// ----------------------------------------------------------------------------
// 6. ParenthesesSubpattern wrapper — single-alternative group around a literal
//    or class. Prefilter recurses through these.
// ----------------------------------------------------------------------------
{
    let re = /(\u{1F0A1})|(\u{1F0B1})|(\u{1F0C1})/u;
    let m = re.exec("\u{1F0B1}");
    shouldBe(m !== null, true, "paren: matched");
    shouldBe(m[0], "\u{1F0B1}", "paren: full match");
    shouldBe(m[1], undefined, "paren: cap1 empty");
    shouldBe(m[2], "\u{1F0B1}", "paren: cap2 captured");
    shouldBe(m[3], undefined, "paren: cap3 empty");
    shouldBe(re.test("\u{1F0A2}"), false, "paren: same lead not enumerated");
}

// ----------------------------------------------------------------------------
// 7. ParenthesesSubpattern with multiple internal alternatives — must NOT be
//    transparent for the prefilter (the body-alt prefilter only recurses into
//    single-alternative groups). Match correctness is what we check here.
// ----------------------------------------------------------------------------
{
    let re = /(\u{1F0A1}|\u{1F0B1})\u{1F0C1}/u;
    shouldBe(re.test("\u{1F0A1}\u{1F0C1}"), true, "multi-alt-paren: A then C");
    shouldBe(re.test("\u{1F0B1}\u{1F0C1}"), true, "multi-alt-paren: B then C");
    shouldBe(re.test("\u{1F0C1}\u{1F0C1}"), false, "multi-alt-paren: C then C");
    shouldBe(re.test("\u{1F0A1}\u{1F0D1}"), false, "multi-alt-paren: A then D");
}

// ----------------------------------------------------------------------------
// 8. Negative — inverted character class must disable the per-term shared-lead
//    fast path (and the prefilter must not over-reject).
// ----------------------------------------------------------------------------
{
    let re = /[^\u{1F0A1}-\u{1F0DE}]/u;
    shouldBe(re.test("\u{1F0A0}"), true, "inverted: just below range");
    shouldBe(re.test("A"), true, "inverted: BMP");
    shouldBe(re.test("\u{1F0A1}"), false, "inverted: in range");
    shouldBe(re.test("\u{1F0DE}"), false, "inverted: range end");
}

// ----------------------------------------------------------------------------
// 9. Negative — BMP-only alternation. Must not engage shared-lead path; must
//    still match correctly under /u.
// ----------------------------------------------------------------------------
{
    let re = /A|B|C/u;
    shouldBe(re.test("A"), true, "bmp: A");
    shouldBe(re.test("D"), false, "bmp: D");
}

// ----------------------------------------------------------------------------
// 10. /v flag (UnicodeSets). The optimization is gated on m_pattern.eitherUnicode()
//     which is true for both /u and /v.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|\u{1F0B1}|\u{1F0C1}/v;
    shouldBe(re.test("\u{1F0A1}"), true, "/v literal-alt: A");
    shouldBe(re.test("\u{1F0B1}"), true, "/v literal-alt: B");
    shouldBe(re.test("\u{1F0A2}"), false, "/v literal-alt: not enumerated");
    shouldBe(re.test("\u{1F600}"), false, "/v literal-alt: same lead diff trail");
}

// ----------------------------------------------------------------------------
// 11. ignoreCase — non-BMP characters that are CanonicalizeUnique survive as
//     PatternCharacter. Mathematical italic codepoints are unique-cased (no
//     case fold equivalent).
// ----------------------------------------------------------------------------
{
    let re = /\u{1D400}|\u{1D401}/iu;  // 𝐀 𝐁 — bold mathematical A/B
    shouldBe(re.test("\u{1D400}"), true, "iu unique: A");
    shouldBe(re.test("\u{1D401}"), true, "iu unique: B");
    shouldBe(re.test("\u{1D402}"), false, "iu unique: C (not enumerated)");
    // Same lead 0xD835, but trail differs.
    shouldBe(re.test("\u{1D403}"), false, "iu unique: same lead, different trail");
}

// ----------------------------------------------------------------------------
// 12. Quantifiers on the first term — quantityMinCount must be > 0 for the
//     prefilter to engage. quantityMinCount = 0 (e.g., \u{1F0A1}?) means the
//     first consumed character isn't necessarily the literal — must still
//     match correctly even if prefilter is skipped.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}?\u{1F0B1}/u;
    shouldBe(re.test("\u{1F0B1}"), true, "?-quant: B alone");
    shouldBe(re.test("\u{1F0A1}\u{1F0B1}"), true, "?-quant: A then B");
    shouldBe(re.test("\u{1F0A1}"), false, "?-quant: A alone");
}

// ----------------------------------------------------------------------------
// 13. Fixed-count quantifier > 1 on the leading term — exercises the
//     generateCharacterClassFixed shared-lead inner loop.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}-\u{1F0DE}]{4}/u;
    shouldBe(re.test("\u{1F0A1}\u{1F0B1}\u{1F0C1}\u{1F0D1}"), true, "fixed-4: matches");
    shouldBe(re.test("\u{1F0A1}\u{1F0B1}\u{1F0C1}"), false, "fixed-4: only 3");
    shouldBe(re.test("\u{1F0A1}\u{1F0B1}\u{1F0C1}A"), false, "fixed-4: 3 + BMP");
    shouldBe(re.test("\u{1F0A1}\u{1F0B1}\u{1F0C1}\u{1F600}"), false, "fixed-4: 3 + same-lead other");
}

// ----------------------------------------------------------------------------
// 14. Greedy quantifier on the leading term — exercises the
//     generateCharacterClassGreedy shared-lead path.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}-\u{1F0DE}]+/u;
    let s = "\u{1F0A1}\u{1F0B1}\u{1F0C1}\u{1F0D1}foo";
    let m = re.exec(s);
    shouldBe(m !== null, true, "greedy: matched");
    shouldBe(m[0], "\u{1F0A1}\u{1F0B1}\u{1F0C1}\u{1F0D1}", "greedy: full prefix");

    let re2 = /[\u{1F0A1}-\u{1F0DE}]+/u;
    shouldBe(re2.test("foo"), false, "greedy: BMP only");
    shouldBe(re2.test("\u{1F600}"), false, "greedy: same lead, different trail");
}

// ----------------------------------------------------------------------------
// 15. Non-anchored search — string starts with non-matching content; the
//     prefilter advances by the right amount and the actual match is found
//     deeper in the haystack.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|\u{1F0B1}/u;
    let prefix = "abc\u{1F600}def🄀ghi";  // includes mixed-lead non-BMP and \u{1F100}
    let target = prefix + "\u{1F0B1}tail";
    let m = re.exec(target);
    shouldBe(m !== null, true, "advance: matched");
    shouldBe(m[0], "\u{1F0B1}", "advance: full match");
    shouldBe(m.index, prefix.length, "advance: index correct");
}

// ----------------------------------------------------------------------------
// 16. Sticky and global flags — sticky disables the prefilter via the
//     `!m_pattern.sticky()` guard at the BodyAlternativeBegin site. Match
//     semantics must remain correct.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|\u{1F0B1}/uy;
    re.lastIndex = 0;
    shouldBe(re.test("\u{1F0B1}foo"), true, "sticky: at 0");
    re.lastIndex = 0;
    shouldBe(re.test("foo\u{1F0B1}"), false, "sticky: not at 0");
    re.lastIndex = 3;
    shouldBe(re.test("foo\u{1F0B1}"), true, "sticky: at 3");
}

// ----------------------------------------------------------------------------
// 17. Capture preservation — the prefilter is just a fast reject; once we
//     fall through to per-alternative codegen, captures must be filled
//     correctly.
// ----------------------------------------------------------------------------
{
    let re = /(\u{1F0A1})|(\u{1F0B1})|(\u{1F0C1})/u;
    let m = re.exec("xx\u{1F0C1}yy");
    shouldBe(m !== null, true, "capture: matched");
    shouldBe(m[0], "\u{1F0C1}", "capture: full match");
    shouldBe(m[1], undefined, "capture: cap1 empty");
    shouldBe(m[2], undefined, "capture: cap2 empty");
    shouldBe(m[3], "\u{1F0C1}", "capture: cap3 set");
    shouldBe(m.index, 2, "capture: index");
}

// ----------------------------------------------------------------------------
// 18. JIT-tier coverage — recompile and re-execute many times so the regex
//     escapes the bytecode interpreter and reaches the JIT, then matches both
//     fast-path acceptances and prefilter rejections.
// ----------------------------------------------------------------------------
{
    let re = /\u{1F0A1}|\u{1F0B1}|\u{1F0C1}|[\u{1F0D1}-\u{1F0DE}]/u;
    let pos = ["\u{1F0A1}", "\u{1F0B1}", "\u{1F0C1}", "\u{1F0D5}"];
    let neg = ["A", "\u{1F600}", "\u{1F0DF}", "\u{1F0A0}", ""];
    for (let i = 0; i < 5000; ++i) {
        for (let p of pos) {
            if (!re.test(p))
                throw new Error("hot: should match " + p);
        }
        for (let n of neg) {
            if (re.test(n))
                throw new Error("hot: should reject " + JSON.stringify(n));
        }
    }
}

// ----------------------------------------------------------------------------
// 19. Slow-path trail-only class — multiple ranges. The synthetic trail-only
//     class has m_rangesUnicode.size() >= 2, which fails both fast-path
//     detectors in readPairAndMatchTrailForSharedLead and falls through to
//     matchCharacterClass. matchCharacterClass asserts that m_rangesUnicode is
//     sorted, so this exercises the std::ranges::sort on m_rangesUnicode in
//     buildTrailsOnlyClass.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}-\u{1F0A3}\u{1F0B1}-\u{1F0B3}\u{1F0C1}-\u{1F0C3}]/u;

    // Members of the union — must match.
    for (let cp of [0x1F0A1, 0x1F0A2, 0x1F0A3,
                    0x1F0B1, 0x1F0B2, 0x1F0B3,
                    0x1F0C1, 0x1F0C2, 0x1F0C3]) {
        shouldBe(re.test(String.fromCodePoint(cp)), true, "slow-multi-range: " + cp.toString(16));
    }

    // Same lead 0xD83C, trails NOT in any range — must reject.
    for (let cp of [0x1F0A0, 0x1F0A4, 0x1F0AF,
                    0x1F0B0, 0x1F0B4, 0x1F0BF,
                    0x1F0C0, 0x1F0C4, 0x1F0FF,
                    0x1F600, 0x1F300]) {
        shouldBe(re.test(String.fromCodePoint(cp)), false, "slow-multi-range: not in set " + cp.toString(16));
    }

    // Different lead surrogate — must reject (lead check kicks before trail).
    shouldBe(re.test("\u{20000}"), false, "slow-multi-range: different lead");

    // BMP haystack — no surrogate pair.
    shouldBe(re.test("ABC"), false, "slow-multi-range: BMP");
}

// ----------------------------------------------------------------------------
// 20. Slow-path trail-only class — mixed matches + ranges. Both
//     m_matchesUnicode and m_rangesUnicode are populated on the synthetic
//     class, so neither fast-path detector fires. Exercises the unifyMatches /
//     unifyRanges logic in matchCharacterClass against the synthetic class.
// ----------------------------------------------------------------------------
{
    // \u{1F0A1} → match; \u{1F0A5}-\u{1F0A8} → range; \u{1F0AA} → match.
    // After dedup/canonicalization the source class will have m_matchesUnicode
    // = {1F0A1, 1F0AA} and m_rangesUnicode = {[1F0A5,1F0A8]}.
    let re = /[\u{1F0A1}\u{1F0A5}-\u{1F0A8}\u{1F0AA}]/u;

    for (let cp of [0x1F0A1, 0x1F0A5, 0x1F0A6, 0x1F0A7, 0x1F0A8, 0x1F0AA])
        shouldBe(re.test(String.fromCodePoint(cp)), true, "slow-mixed: " + cp.toString(16));

    for (let cp of [0x1F0A0, 0x1F0A2, 0x1F0A3, 0x1F0A4, 0x1F0A9, 0x1F0AB, 0x1F0DE])
        shouldBe(re.test(String.fromCodePoint(cp)), false, "slow-mixed: not in set " + cp.toString(16));

    shouldBe(re.test("A"), false, "slow-mixed: BMP");
    shouldBe(re.test("\u{20000}"), false, "slow-mixed: different lead");
}

// ----------------------------------------------------------------------------
// 21. Slow-path with quantifier — exercises the slow path inside a
//     fixed-count quantifier loop, where the synthetic class is built once and
//     read on every iteration. If the sort were missing or wrong, the
//     std::is_sorted assertion in matchCharacterClass would fire on debug
//     builds; on release we'd silently miscompile and the wrong characters
//     would match.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}-\u{1F0A3}\u{1F0B1}-\u{1F0B3}\u{1F0C1}-\u{1F0C3}]{3}/u;

    let goodSeqs = [
        "\u{1F0A1}\u{1F0B2}\u{1F0C3}",
        "\u{1F0A2}\u{1F0A3}\u{1F0B1}",
        "\u{1F0C1}\u{1F0C2}\u{1F0C3}",
    ];
    for (let s of goodSeqs)
        shouldBe(re.test(s), true, "slow-quant: should match " + s.codePointAt(0).toString(16) + "..");

    let badSeqs = [
        "\u{1F0A4}\u{1F0B2}\u{1F0C3}",  // first char outside
        "\u{1F0A1}\u{1F0B4}\u{1F0C3}",  // middle char outside
        "\u{1F0A1}\u{1F0B2}\u{1F0DE}",  // last char outside, same lead
        "\u{1F0A1}\u{1F0B2}A",          // last char BMP
    ];
    for (let s of badSeqs)
        shouldBe(re.test(s), false, "slow-quant: should reject " + s);
}

// ----------------------------------------------------------------------------
// 22. Slow-path JIT-tier hot loop — drives the slow-path codegen across
//     thousands of iterations to escape the interpreter and stress the
//     compiled JIT code on a class large enough to bypass both fast paths.
// ----------------------------------------------------------------------------
{
    let re = /[\u{1F0A1}-\u{1F0A3}\u{1F0A5}\u{1F0A7}\u{1F0B1}-\u{1F0B3}\u{1F0C1}-\u{1F0C3}]/u;
    let pos = ["\u{1F0A1}", "\u{1F0A2}", "\u{1F0A3}", "\u{1F0A5}", "\u{1F0A7}",
               "\u{1F0B1}", "\u{1F0B2}", "\u{1F0B3}", "\u{1F0C1}", "\u{1F0C2}", "\u{1F0C3}"];
    let neg = ["\u{1F0A0}", "\u{1F0A4}", "\u{1F0A6}", "\u{1F0A8}", "\u{1F0AF}",
               "\u{1F0B0}", "\u{1F0B4}", "\u{1F0C0}", "\u{1F0C4}", "\u{1F600}", "A", ""];
    for (let i = 0; i < 3000; ++i) {
        for (let p of pos) {
            if (!re.test(p))
                throw new Error("slow-hot: should match " + p);
        }
        for (let n of neg) {
            if (re.test(n))
                throw new Error("slow-hot: should reject " + JSON.stringify(n));
        }
    }
}
