//@ runDefault

// Exercises Greedy variable-count parens with min > 0 in heavy-backtracking
// scenarios that drive Begin.bt's failure exit (where the new op.m_jumps.link
// lives) and the "decrement and try fewer iterations" success paths. Validates
// that the JIT codegen for ParenthesesSubpatternBegin.backtrack matches the
// reference engine.

function shouldBe(actual, expected, label) {
    let a = JSON.stringify(actual);
    let e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("FAIL " + label + ": expected " + e + " got " + a);
}

// 1. Min not reachable: pure failure path through Begin.bt.
shouldBe(/(a+){3,}/.exec("aa"), null, "fail: 2 a's, need 3 iters of a+");
shouldBe(/(a+){3,5}b/.exec("aab"), null, "fail: 2 a's then b, need 3 iters");
shouldBe(/(a+){5,}/.exec("aaaa"), null, "fail: 4 a's, need 5 iters");

// 2. Heavy content backtracking driven by post-parens literal mismatch.
shouldBe(/(a+){2,5}b/.exec("aaaaaab"), ["aaaaaab", "a"], "6 a's + b, min=2 max=5");
shouldBe(/(a+){3,5}b/.exec("aaaaaab"), ["aaaaaab", "a"], "6 a's + b, min=3 max=5");
shouldBe(/(a+){3,5}b/.exec("aaaaaaab"), ["aaaaaaab", "a"], "7 a's + b, min=3 max=5");

// 3. Post-parens that requires backtracking down to exactly min.
shouldBe(/(a+){2,5}aab/.exec("aaaaaab"), ["aaaaaab", "a"], "post-parens consumes aab");

// 4. Multi-alternative inner with min > 0.
shouldBe(/(a+|b+){2,4}c/.exec("aabbc"), ["aabbc", "bb"], "multi-alt: aa then bb then c");
shouldBe(/(a+|b+){2,4}c/.exec("aabbbbc"), ["aabbbbc", "bbbb"], "multi-alt: aa then bbbb then c");
shouldBe(/(a+|b+){3,}/.exec("ab"), null, "multi-alt fail: only 1 alternation");

// 5. Nested greedy with min > 0 at multiple levels.
shouldBe(/((a+){2,3}){2,3}b/.exec("aaaaaab"), ["aaaaaab", "aa", "a"], "nested {2,3} of {2,3}");
shouldBe(/((a+){2,4}){2,3}/.exec("aaaaaa"), ["aaaaaa", "aa", "a"], "nested {2,3} inner {2,4}");
shouldBe(/((a+){2,3}){3,}b/.exec("aab"), null, "nested fail: outer needs 3 iters");

// 6. Greedy with min > 0 followed by another greedy.
shouldBe(/(a+){2,4}(a+)/.exec("aaaaa"), ["aaaaa", "a", "a"], "two greedies");
shouldBe(/(a+){2,3}(a*)b/.exec("aaaab"), ["aaaab", "a", ""], "first eats most, second rest");

// 7. Quantified that itself is inside another greedy.
shouldBe(/((?:a){2,3}b)+c/.exec("aabaaabc"), ["aabaaabc", "aaab"], "outer + over inner {2,3}b");
shouldBe(/((?:a){2,3}b)+c/.exec("ab"), null, "outer + fail");

// 8. min == max for variable-count form.
shouldBe(/(a+){3,3}b/.exec("aaab"), ["aaab", "a"], "min == max success");
shouldBe(/(a+){3,3}b/.exec("aab"), null, "min == max fail");
