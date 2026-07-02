// Regression coverage for YarrJIT auto-possessification: a Greedy single-character term
// (PatternCharacter / CharacterClass) immediately followed by a mandatory term whose first
// character is disjoint from the greedy term's set is matched possessively (the JIT skips the
// futile give-back/retry backtracking). Results must be identical to a fully-backtracking
// engine, so every case below is also validated against the Yarr interpreter and V8.
// Throws on mismatch; no output on success.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: got ${actual}, expected ${expected}`);
}

function match(re, str) {
    let m = re.exec(str);
    return m === null ? "null" : JSON.stringify(Array.from(m).map(x => x === undefined ? null : x)) + "@" + m.index;
}

function test(reSource, flags, str, expected) {
    // Compile once and run hot so the JIT tiers up and the possessive backtrack path runs.
    let re = new RegExp(reSource, flags);
    for (let i = 0; i < 200; ++i)
        shouldBe(match(re, str), expected);
}

// --- Greedy followed by a disjoint mandatory single character (the possessified case) ---
// Successful matches must still produce the correct longest match.
test("[0-9a-f]{1,4}:", "", "abcd:", '["abcd:"]@0');
test("[0-9a-f]{1,4}:", "", "a:", '["a:"]@0');
test("^([0-9a-f]{1,4}:)+x$", "", "abcd:1:ef01:x", '["abcd:1:ef01:x","ef01:"]@0');
test("^[0-9]+\\.[0-9]+$", "", "123.456", '["123.456"]@0');
test("^[a-z]+-end$", "", "hello-end", '["hello-end"]@0');
test("a+b", "", "aaab", '["aaab"]@0');

// --- Failing inputs: possessification must not change the (non-)match ---
test("^([0-9a-f]{1,8}-)+x$", "", "abcdef12-abcdef12-y", "null");
test("[0-9a-f]{1,4}:", "", "abcd", "null");
test("^[0-9]+\\.[0-9]+$", "", "123.", "null");
test("a+b", "", "aaac", "null");
test("^([0-9a-f]{1,4}:){7}(?:[0-9a-f]{1,4}|:)$", "", "2001:db8:0:1:1:1:1::1", "null");

// --- Overlapping follower: MUST NOT be possessified (give-back is required) ---
test("a+a$", "", "aaaa", '["aaaa"]@0');         // 'a+' followed by 'a' (same char): must give back one.
test("a+ab", "", "aaab", '["aaab"]@0');         // 'a+' must yield the 'a' that 'ab' needs.
test("[0-9]+[0-9a-f]+g$", "", "12ffg", '["12ffg"]@0');   // overlapping classes.
test("[0-9]+[5-9]z$", "", "1239z", '["1239z"]@0');       // overlapping ranges: give-back needed.

// --- ignoreCase: class folds in case; follower case-insensitive ---
test("[a-z]+;", "i", "ABCdef;", '["ABCdef;"]@0');
test("[a-z]+X", "i", "abcx", '["abcx"]@0');     // follower 'X' under /i matches 'x' which IS in [a-z]: must not possessify.

// --- Disjoint follower under /i where follower has no case (digit/punct) ---
test("[a-z]+9", "i", "ABc9", '["ABc9"]@0');

// --- Greedy CharacterClass followed by disjoint class-shaped literal, with captures intact ---
test("^(\\d{1,3})\\.(\\d{1,3})$", "", "12.250", '["12.250","12","250"]@0');

// --- Quantified parentheses wrapping a possessive inner term (IPv6-like) ---
test("^(?:[0-9a-f]{1,4}:){2}[0-9a-f]{1,4}$", "", "ab:cd:ef", '["ab:cd:ef"]@0');
test("^(?:[0-9a-f]{1,4}:){2}[0-9a-f]{1,4}$", "", "ab:cd:", "null");

// === Unicode (/u) and unicodeSets (/v): surrogate-pair decode and case-fold soundness ===
// All expected values below were cross-checked against V8 (a fully-backtracking engine).

// --- Non-BMP greedy PatternCharacter + disjoint BMP follower: give-back must be doubled
//     (2 code units per matched code point). ---
test("\\u{1F600}+x", "u", "\u{1F600}\u{1F600}\u{1F600}x", '["\u{1F600}\u{1F600}\u{1F600}x"]@0');
test("\\u{1F600}+x", "u", "\u{1F600}\u{1F600}\u{1F600}y", "null"); // possessive give-back is futile.

// --- Non-BMP, fixed-width CharacterClass greedy + disjoint follower (count << 1 give-back). ---
test("[\\u{1F600}-\\u{1F610}]+!", "u", "\u{1F600}\u{1F601}\u{1F602}!", '["\u{1F600}\u{1F601}\u{1F602}!"]@0');
test("[\\u{1F600}-\\u{1F610}]+!", "u", "\u{1F600}\u{1F601}\u{1F602}?", "null");
// Bounded greedy: possessive at index 0 fails, engine still finds the later non-anchored match.
test("[\\u{1F600}-\\u{1F610}]{1,3}!", "u", "\u{1F600}\u{1F601}\u{1F602}\u{1F603}!", '["\u{1F601}\u{1F602}\u{1F603}!"]@2');

// --- Variable-width class (mixes BMP + non-BMP): NOT fixed width, so the JIT must fall back to
//     the normal per-step backtrack under surrogate decoding. Result must still be correct. ---
test("[a\\u{1F600}]+!", "u", "a\u{1F600}a!", '["a\u{1F600}a!"]@0');
test("[a\\u{1F600}]+!", "u", "a\u{1F600}a?", "null");

// --- Inverted class under /u is variable width too (JIT falls back); must stay correct. ---
test("[^0-9]+5", "u", "abc5", '["abc5"]@0');
test("[^0-9]+5", "u", "ab\u{1F600}c5", '["ab\u{1F600}c5"]@0');
test("[^0-9]+5", "u", "abc6", "null");

// --- Case-fold soundness under /iu: a disjoint follower whose Unicode fold reaches a non-ASCII
//     char (Kelvin U+212A folds to 'k', long-s U+017F folds to 's'). The greedy [0-9]/[a-z] class
//     genuinely rejects those, so possessification is valid and the follower still matches them. ---
test("[0-9]+k", "iu", "123K", '["123K"]@0'); // follower /k/iu matches Kelvin.
test("[0-9]+s", "iu", "12ſ", '["12ſ"]@0');   // follower /s/iu matches long-s.
test("[0-9]+k", "iu", "123k", '["123k"]@0');

// --- The dangerous case that must NOT be possessified: under /iu, [K] folds to {k,K,Kelvin},
//     so it DOES overlap the ASCII follower 'k' and give-back is required. (A naive analysis that
//     only saw the literal Kelvin code point would wrongly possessify and return null.) ---
test("[\\u212A]+k", "iu", "KKKk", '["KKKk"]@0');
test("[\\u212A]+k", "iu", "KKk", '["KKk"]@0');

// --- /v string-disjunction class: [\q{..}] is decomposed into a group, so the inner single-char
//     class is FixedCount (never the greedy term) and the strings are never possessified away. ---
test("[\\q{ab}c]+d", "v", "ababccd", '["ababccd"]@0');
test("[\\q{xy}a]+b", "v", "xyaab", '["xyaab"]@0');
test("[\\q{ab}]+a", "v", "ababa", '["ababa"]@0');

// --- Dot (non-dotAll) is the newline class inverted: '.' rejects '\n', so '.+\n' is possessifiable.
//     This is the BMP path (one code unit per match). ---
test(".+\\n", "", "abc\ndef", '["abc\\n"]@0');
test(".+\\n", "", "abcdef", "null");
test("[^0-9]+5", "", "abc5", '["abc5"]@0');
test("[^0-9]+5", "", "abc6", "null");
