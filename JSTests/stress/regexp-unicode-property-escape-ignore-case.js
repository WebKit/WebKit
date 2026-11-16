function shouldBe(actual, expected, description) {
    if (actual !== expected)
        throw new Error(`${description}: expected ${expected} but got ${actual}`);
}

function test(pattern, flags, input, expected) {
    const regexp = new RegExp("^(?:" + pattern + ")$", flags);
    shouldBe(regexp.test(input), expected, `/${pattern}/${flags}.test(${JSON.stringify(input)})`);
}

// \p{...} matches case-insensitively in both u and v modes.
for (const flags of ["iu", "iv"]) {
    test("\\p{Lu}", flags, "a", true);
    test("\\p{Lu}", flags, "A", true);
    test("\\p{Lu}", flags, "1", false);
    test("\\p{Ll}", flags, "A", true);
    test("\\p{gc=Lu}", flags, "a", true);
    test("\\p{General_Category=Ll}", flags, "A", true);
    test("\\p{Uppercase}", flags, "a", true);
    test("\\p{Lowercase}", flags, "A", true);
    test("[\\p{Lu}]", flags, "a", true);
    test("[\\p{Lu}0]", flags, "a", true);
    test("[^\\p{Ll}]", flags, "A", false);
    test("[^\\p{Ll}]", flags, "1", true);
    test("\\p{Lu}+", flags, "aBcD", true);

    // U+212A KELVIN SIGN and U+017F LATIN SMALL LETTER LONG S fold to k and s.
    test("\\p{ASCII}", flags, "\u212A", true);
    test("\\p{ASCII}", flags, "\u017F", true);
    test("[\\p{ASCII}]", flags, "\u212A", true);
    // U+00B5 MICRO SIGN folds to U+03BC GREEK SMALL LETTER MU.
    test("\\p{sc=Greek}", flags, "\u00B5", true);
    test("\\p{sc=Latin}", flags, "\u212A", true);
    // U+01C5 (Lt) folds to U+01C6 (Ll); U+01C4 (Lu) folds there too.
    test("\\p{Lt}", flags, "\u01C4", true);
    test("\\p{Lt}", flags, "\u01C6", true);
    // Non-BMP: U+10400 DESERET CAPITAL LETTER LONG I / U+10428 DESERET SMALL LETTER LONG I.
    test("\\p{Lu}", flags, "\u{10428}", true);
    test("\\p{Ll}", flags, "\u{10400}", true);

    // Properties without cased code points are unaffected.
    test("\\p{Nd}", flags, "a", false);
    test("\\P{Nd}", flags, "a", true);
    test("\\P{Nd}", flags, "1", false);

    // The i modifier alone enables folding, and removing it disables folding.
    test("(?i:\\p{Lu})", flags.replace("i", ""), "a", true);
    test("(?i:[^\\p{Ll}])", flags.replace("i", ""), "A", false);
    test("(?-i:\\p{Lu})", flags, "a", false);
    test("(?-i:[^\\p{Ll}])", flags, "A", true);
}

// In u mode, \P{...} is complemented before case folding, so it matches any code point that has a case variant outside the property.
test("\\P{Lu}", "iu", "A", true);
test("\\P{Lu}", "iu", "a", true);
test("\\P{Lu}", "iu", "1", true);
test("\\P{Ll}", "iu", "a", true);
test("[\\P{Ll}]", "iu", "a", true);
test("[\\P{Ll}0]", "iu", "a", true);
test("[^\\P{Ll}]", "iu", "a", false);
test("[^\\P{Ll}]", "iu", "A", false);
test("\\P{Lt}", "iu", "\u01C5", true);
test("\\P{ASCII}", "iu", "k", true);
test("\\P{ASCII}", "iu", "\u212A", true);
test("\\P{ASCII}", "iu", "!", false);
test("(?i:\\P{Lu})", "u", "A", true);
test("(?-i:\\P{Lu})", "iu", "A", false);
test(".(?<=\\P{Nd})x", "iu", "\u{1F600}x", true);
test(".(?<=\\P{Nd})x", "iu", "1x", false);
test(".(?<!\\P{L})x", "iu", "\u{1F600}x", false);
test(".(?<!\\P{L})x", "iu", "\u{10400}x", true);

// In v mode, \P{...} and [^...] are complemented after case folding, so they never match a case variant of a member.
test("\\P{Lu}", "iv", "A", false);
test("\\P{Lu}", "iv", "a", false);
test("\\P{Lu}", "iv", "1", true);
test("\\P{Ll}", "iv", "A", false);
test("[\\P{Ll}]", "iv", "A", false);
test("[\\P{Ll}0]", "iv", "A", false);
test("[0\\P{Ll}]", "iv", "A", false);
test("[^\\P{Ll}]", "iv", "a", true);
test("[^\\P{Ll}]", "iv", "A", true);
test("[^\\P{Ll}]", "iv", "1", false);
test("\\P{Lt}", "iv", "\u01C5", false);
test("\\P{ASCII}", "iv", "k", false);
test("\\P{ASCII}", "iv", "\u212A", false);
test("\\P{ASCII}", "iv", "\u00E9", true);
test("[\\P{L}]", "iv", "\u017F", false);
test("[\\P{L}]", "iv", "\u03C2", false);
test("[\\P{L}]", "iv", "\u01C5", false);
test("[\\P{L}]", "iv", "_", true);
test("(?i:\\P{Lu})", "v", "A", false);
test("(?-i:\\P{Lu})", "iv", "a", true);

// v-mode set operations see the case-folded property.
test("[\\p{Any}&&\\P{Lu}]", "iv", "a", false);
test("[\\p{Any}&&\\P{Lu}]", "iv", "1", true);
test("[\\p{Any}--\\p{Lu}]", "iv", "a", false);
test("[\\p{Any}--\\P{Lu}]", "iv", "a", true);
test("[\\p{Lu}&&\\p{Ll}]", "iv", "a", true);
test("[\\p{Lu}&&\\p{Ll}]", "iv", "A", true);
test("[\\p{Lu}&&\\p{Ll}]", "iv", "\u00AA", false);
test("[\\p{Lu}--\\p{Ll}]", "iv", "A", false);
test("[\\p{L}--\\p{Lu}]", "iv", "a", false);
test("[\\p{L}--\\p{Lu}]", "iv", "\u00AA", true);
test("[\\p{Lu}--[a-z]]", "iv", "K", false);
test("[\\p{Lu}--[a-z]]", "iv", "\u212A", false);
test("[\\p{Lu}--[a-z]]", "iv", "\u00C5", true);
test("[\\P{ASCII}&&[A-Z]]", "iv", "\u212A", false);
test("[[\\P{L}]&&[\\P{Ll}]]", "iv", "A", false);
test("[[\\P{L}]&&[\\P{Ll}]]", "iv", "1", true);
test("[^[\\p{L}]--\\q{a|K}]", "iv", "A", true);
test("[^[\\p{L}]--\\q{a|K}]", "iv", "\u212A", true);
test("[^[\\p{L}]--\\q{a|K}]", "iv", "b", false);
