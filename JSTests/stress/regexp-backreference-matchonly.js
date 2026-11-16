// Test backreferences in MatchOnly mode (test() method).
// This tests the JIT implementation of backreferences when m_regs.output is not available.

function testMatchOnly(re, str, expected, description) {
    let result = re.test(str);
    if (result !== expected)
        throw new Error(`${description || re.toString()}.test("${str}") = ${result}, expected ${expected}`);
}

// ==== Basic backreferences ====

// Simple backreference - match
testMatchOnly(/(.)\1/, "aa", true, "simple backref match");
testMatchOnly(/(.)\1/, "bb", true, "simple backref match 2");
testMatchOnly(/(.)\1/, "11", true, "simple backref match digit");

// Simple backreference - no match
testMatchOnly(/(.)\1/, "ab", false, "simple backref no match");
testMatchOnly(/(.)\1/, "a", false, "simple backref too short");
testMatchOnly(/(.)\1/, "", false, "simple backref empty");

// ==== Multiple backreferences ====

testMatchOnly(/(.)(.)\1\2/, "abab", true, "multiple backref match");
testMatchOnly(/(.)(.)\1\2/, "abba", false, "multiple backref no match");
testMatchOnly(/(.)(.)\1\2/, "xyzxy", false, "multiple backref partial");

// Three capture groups
testMatchOnly(/(.)(.)(.)(\1\2\3)/, "abcabc", true, "three groups match");
testMatchOnly(/(.)(.)(.)(\1\2\3)/, "abcdef", false, "three groups no match");

// ==== Quantified backreferences ====

// Greedy quantifiers
testMatchOnly(/(.)\1+/, "aaa", true, "backref + match");
testMatchOnly(/(.)\1+/, "a", false, "backref + no match (needs 2+)");
testMatchOnly(/(.)\1*/, "a", true, "backref * match (0 repeats ok)");
testMatchOnly(/(.)\1*/, "aaa", true, "backref * match multiple");
testMatchOnly(/(.)\1?/, "a", true, "backref ? match (0 ok)");
testMatchOnly(/(.)\1?/, "aa", true, "backref ? match (1 ok)");
testMatchOnly(/(.)\1{2}/, "aaa", true, "backref {2} match");
testMatchOnly(/(.)\1{2}/, "aa", false, "backref {2} no match");
testMatchOnly(/(.)\1{2,4}/, "aaaa", true, "backref {2,4} match");
testMatchOnly(/(.)\1{2,4}/, "aa", false, "backref {2,4} no match (too few)");

// Non-greedy quantifiers
testMatchOnly(/(.)\1+?/, "aaa", true, "backref +? match");
testMatchOnly(/(.)\1*?/, "a", true, "backref *? match");
testMatchOnly(/(.)\1??/, "a", true, "backref ?? match");

// ==== Named capture groups ====

testMatchOnly(/(?<char>.)\k<char>/, "aa", true, "named backref match");
testMatchOnly(/(?<char>.)\k<char>/, "ab", false, "named backref no match");
testMatchOnly(/(?<a>.)(?<b>.)\k<a>\k<b>/, "abab", true, "multiple named backref match");
testMatchOnly(/(?<a>.)(?<b>.)\k<a>\k<b>/, "abba", false, "multiple named backref no match");

// ==== Duplicate named capture groups ====

testMatchOnly(/(?:(?<x>a)|(?<x>b))\k<x>/, "aa", true, "duplicate named group match a");
testMatchOnly(/(?:(?<x>a)|(?<x>b))\k<x>/, "bb", true, "duplicate named group match b");
testMatchOnly(/(?:(?<x>a)|(?<x>b))\k<x>/, "ab", false, "duplicate named group no match");
testMatchOnly(/(?:(?<x>a)|(?<x>b))\k<x>/, "ba", false, "duplicate named group no match 2");

// More complex duplicate named groups
testMatchOnly(/(?:(?:(?<x>a)|(?<x>b))\k<x>){2}/, "aabb", true, "repeated duplicate named group");
testMatchOnly(/(?:(?:(?<x>a)|(?<x>b))\k<x>){2}/, "aaab", false, "repeated duplicate named group no match");

// ==== Nested parentheses ====

// /((.))\1\2/ captures one char in group 1 and 2 (same char), then expects that char twice more
testMatchOnly(/((.))\1\2/, "aaaa", true, "nested groups match");
testMatchOnly(/((.))\1\2/, "abab", false, "nested groups no match (different chars)");
testMatchOnly(/((.))\1\2/, "abcd", false, "nested groups no match");

// /((.)(.))\1/ captures two chars in group 1, then expects the same two chars
testMatchOnly(/((.)(.))\1/, "abab", true, "nested with multiple chars");
testMatchOnly(/((.)(.))\1/, "abcabc", false, "nested with multiple chars no match");

// ==== Case insensitive ====

testMatchOnly(/(.)\1/i, "aA", true, "ignoreCase match");
testMatchOnly(/(.)\1/i, "Aa", true, "ignoreCase match reverse");
testMatchOnly(/(.)\1/i, "ab", false, "ignoreCase no match");
testMatchOnly(/(?<x>.)\k<x>/i, "aA", true, "named ignoreCase match");

// ==== Unicode mode ====

testMatchOnly(/(.)\1/u, "aa", true, "unicode mode match");
testMatchOnly(/(.)\1/u, "ab", false, "unicode mode no match");

// Non-BMP characters (surrogate pairs)
testMatchOnly(/(.)\1/u, "\u{1F600}\u{1F600}", true, "emoji match");
testMatchOnly(/(.)\1/u, "\u{1F600}\u{1F601}", false, "emoji no match");
testMatchOnly(/(.)\1/u, "\u{10000}\u{10000}", true, "non-BMP match");

// ==== Mixed with non-capturing groups ====

testMatchOnly(/(?:a)(b)\1/, "abb", true, "non-capture prefix match");
testMatchOnly(/(?:a)(b)\1/, "abc", false, "non-capture prefix no match");
testMatchOnly(/(a)(?:b)\1/, "aba", true, "non-capture middle match");
testMatchOnly(/(a)(?:b)(c)\1\2/, "abcac", true, "mixed groups match");

// ==== Complex patterns (marked.js fences style) ====

testMatchOnly(/(`{3,}).*\1/, "```code```", true, "fences pattern match");
testMatchOnly(/(`{3,}).*\1/, "````code````", true, "fences pattern 4 backticks");
testMatchOnly(/(`{3,}).*\1/, "```code``", false, "fences pattern no match (fewer)");
testMatchOnly(/(`{3,}).*\1/, "```code", false, "fences pattern no match (none)");

// Tilde variant
testMatchOnly(/(~{3,}).*\1/, "~~~code~~~", true, "tilde fences match");
testMatchOnly(/(~{3,}).*\1/, "~~~code~~", false, "tilde fences no match");

// ==== Lookahead with backreference ====

testMatchOnly(/(?=(.)\1)aa/, "aa", true, "lookahead with backref match");
testMatchOnly(/(?=(.)\1)ab/, "ab", false, "lookahead with backref no match");

// ==== Empty capture ====

testMatchOnly(/()\1/, "", true, "empty capture match");
testMatchOnly(/()\1a/, "a", true, "empty capture with char");
testMatchOnly(/()?\1/, "", true, "optional empty capture");

// ==== Backreference to unmatched group ====

testMatchOnly(/(a)?\1/, "aa", true, "optional group matched");
testMatchOnly(/(a)?\1/, "", true, "optional group unmatched (backref matches empty)");
testMatchOnly(/(a)?\1/, "a", true, "optional group captures but backref empty ok");

// ==== Long strings ====

let longStr = "a".repeat(1000);
testMatchOnly(/(.)\1{999}/, longStr, true, "long string match");
testMatchOnly(/(.)\1{1000}/, longStr, false, "long string no match (off by one)");

// Repeated pattern
let repeatedPairs = "ab".repeat(500);
testMatchOnly(/(..)\1{499}/, repeatedPairs, true, "repeated pairs match");

// ==== Backreference at different positions ====

testMatchOnly(/^(.)\1$/, "aa", true, "anchored backref match");
testMatchOnly(/^(.)\1$/, "aaa", false, "anchored backref no match (too long)");
testMatchOnly(/(.)\1$/, "baa", true, "end anchored backref match");
testMatchOnly(/^(.)\1/, "aab", true, "start anchored backref match");

// ==== Word boundaries ====

testMatchOnly(/\b(.)\1\b/, "aa", true, "word boundary backref match");
testMatchOnly(/(\w)\1/, "aa", true, "word char backref match");
testMatchOnly(/(\w)\1/, "a1", false, "word char backref no match");

// ==== Character classes ====

testMatchOnly(/([aeiou])\1/, "ee", true, "vowel backref match");
testMatchOnly(/([aeiou])\1/, "ea", false, "vowel backref no match");
testMatchOnly(/([0-9])\1/, "55", true, "digit class backref match");
testMatchOnly(/([^a])\1/, "bb", true, "negated class backref match");

// ==== Multiline mode ====

testMatchOnly(/^(.)\1/m, "aa\nbb", true, "multiline backref match");
testMatchOnly(/(.)\1$/m, "ab\ncc", true, "multiline end backref match");

// ==== dotAll mode ====

testMatchOnly(/(.)\1/s, "aa", true, "dotAll backref match");
testMatchOnly(/(.).\1/s, "a\na", true, "dotAll with newline match");
testMatchOnly(/(.).\1/s, "a\nb", false, "dotAll with newline no match");

// ==== Stress test: Many capture groups ====

// 10 capture groups
testMatchOnly(/(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)\1\2\3\4\5\6\7\8\9\10/,
    "abcdefghijabcdefghij", true, "10 groups match");
testMatchOnly(/(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)\1\2\3\4\5\6\7\8\9\10/,
    "abcdefghijabcdefghix", false, "10 groups no match");

// ==== Alternation with backreference ====

testMatchOnly(/(a|b)\1/, "aa", true, "alternation backref match a");
testMatchOnly(/(a|b)\1/, "bb", true, "alternation backref match b");
testMatchOnly(/(a|b)\1/, "ab", false, "alternation backref no match");
testMatchOnly(/(a|b)\1/, "ba", false, "alternation backref no match 2");

// ==== Complex alternation ====

testMatchOnly(/(?:(a)|(b))\1\2/, "a", false, "complex alt - partial capture");
testMatchOnly(/((?:a|b)+)\1/, "abab", true, "repeated alt group match");
testMatchOnly(/((?:a|b)+)\1/, "abba", true, "repeated alt group match (bb part)");
testMatchOnly(/((?:a|b)+)\1/, "abcd", false, "repeated alt group no match");

