// Regression coverage for the unified save-at-BEGIN model for quantified
// parenthesised subpatterns in YarrJIT (FixedCount/Greedy/NonGreedy share one
// ParenContext push/pop flow; FixedCount is min == max). Throws on mismatch.
// Expected values were verified against the Yarr interpreter and V8.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: got ${actual}, expected ${expected}`);
}

function match(re, str) {
    let m = re.exec(str);
    return m === null ? "null" : JSON.stringify(Array.from(m).map(x => x === undefined ? null : x)) + "@" + m.index;
}

function test(reSource, flags, str, expected) {
    // Compile once and execute repeatedly so the Yarr JIT tiers up and the
    // JIT-compiled code path (not just the interpreter) is exercised.
    let re = new RegExp(reSource, flags);
    for (let i = 0; i < 200; ++i)
        shouldBe(match(re, str), expected);
}

// --- FixedCount with backtrackable single-alternative content ---
test("(a+){2}b", "", "aaab", '["aaab","a"]@0');
test("(a+){3}", "", "aaaa", '["aaaa","a"]@0');
test("(a+){2}$", "", "aaaa", '["aaaa","a"]@0');
test("(a){3}", "", "aaa", '["aaa","a"]@0');
test("(a){3}", "", "aa", "null");
test("(ab){2,2}", "", "abab", '["abab","ab"]@0');

// --- FixedCount with multiple alternatives (inter-iteration alt retry) ---
test("(a+|b+){2}c", "", "aabbc", '["aabbc","bb"]@0');
test("(a+|b+){2}c", "", "aabc", '["aabc","b"]@0');
test("(a|b){3}", "", "abab", '["aba","a"]@0');
test("(?:(a+)b){2}", "", "abaab", '["abaab","aa"]@0');
test("(?:(a+)b){2}", "", "aabab", '["aabab","a"]@0');

// --- Nested FixedCount / Greedy ---
test("((a+)+){2}", "", "aaaa", '["aaaa","a","a"]@0');
test("(\\w)\\1{2}", "", "aaa", '["aaa","a"]@0');

// --- Greedy / NonGreedy regression guards (must be unaffected) ---
test("(ab|cd)*", "", "abcdab", '["abcdab","ab"]@0');
test("(a+?){2,4}", "", "aaaa", '["aaaa","a"]@0');
test("(a+){2,}", "", "aaaa", '["aaaa","a"]@0');
test("(a*){3}", "", "aaa", '["aaa",""]@0');
test("(a+)*?b", "", "aaab", '["aaab","aaa"]@0');

// --- Backtrack-then-FAIL: backtracking exhausts every iteration distribution and
//     bails out (no trailing match exists). This drives BEGIN.bt all the way down
//     to count == 0 (noPreviousIteration) and exercises the failure-propagation /
//     capture-clearing paths that the "backtrack then succeed" cases above do not. ---

// Single-alt backtrackable FixedCount: every split of the a's across N iterations
// is tried, the trailing 'b' never matches, so the whole match fails.
test("(a+){2}b", "", "aaa", "null");
test("(a+){3}b", "", "aaaa", "null");
test("(a+){4}b", "", "aaaaaa", "null");

// Multi-alternative FixedCount: both alternatives are tried in every iteration
// before bailing out.
test("(a+|b+){2}c", "", "aabb", "null");
test("(a+|b+){2}c", "", "ab", "null");
test("(a+|b+){3}c", "", "aabb", "null");

// Nested FixedCount: inner and outer iteration counts both backtrack to exhaustion.
test("((a+)+){2}b", "", "aaaa", "null");

// FixedCount bails out, then an enclosing alternation succeeds: the failed group's
// captures must be cleared (undefined) before the alternative is taken.
test("(?:(a+){2}b|a+)", "", "aaa", '["aaa",null]@0');
test("((a+){2}b)|(a+)", "", "aaa", '["aaa",null,null,"aaa"]@0');

// Same bail-out chain through the shared code path for Greedy-below-min and
// NonGreedy (both reduce to "re-drive previous content, then fail").
test("(a+){2,}b", "", "aaa", "null");
test("(a+){2,4}?b", "", "aaa", "null");
