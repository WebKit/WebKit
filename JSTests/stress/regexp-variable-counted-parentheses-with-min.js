//@ runDefault

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Test 1: Two quantified parentheses - matching cases
(function() {
    var re = /(x){1,2}(a){2,4}c/;
    shouldBe(re.exec("xaaaac"), ["xaaaac", "x", "a"], "Greedy 2-4 matches 4 a's");
    shouldBe(re.exec("xaaac"), ["xaaac", "x", "a"], "Greedy 2-4 matches 3 a's");
    shouldBe(re.exec("xaac"), ["xaac", "x", "a"], "Greedy 2-4 matches 2 a's");
    // Single non-match is OK, just don't do it in a loop
    shouldBe(re.exec("xac"), null, "Greedy 2-4 fails with only 1 a");
    shouldBe(re.exec("xaaaaac"), null, "Greedy 2-4 on 5 a's fails (extra a before c)");
})();

// Test 2: Two x's with {2,3}
(function() {
    var re = /(x){1,2}(a){2,3}c/;
    shouldBe(re.exec("xxaaac"), ["xxaaac", "x", "a"], "Two x's with three a's");
    shouldBe(re.exec("xxaaaac"), null, "Four a's fails (extra a before c)");
})();

// Test 3: Non-greedy with two quantified parentheses
(function() {
    var re = /(x){1,2}(a){2,4}?c/;
    shouldBe(re.exec("xaac"), ["xaac", "x", "a"], "Non-greedy uses min 2");
    shouldBe(re.exec("xaaac"), ["xaaac", "x", "a"], "Non-greedy forced to use 3");
    shouldBe(re.exec("xaaaac"), ["xaaaac", "x", "a"], "Non-greedy forced to max");
    shouldBe(re.exec("xac"), null, "Non-greedy fails below min");
})();

// Test 4: Non-capturing groups with variable count
(function() {
    var re = /(x){1,2}(?:ab){2,4}c/;
    shouldBe(re.exec("xababababc"), ["xababababc", "x"], "Non-capturing 2-4 matches 4");
    shouldBe(re.exec("xababc"), ["xababc", "x"], "Non-capturing 2-4 matches min 2");
})();

// Test 5: Large min values
(function() {
    var re = /(x){1,2}(a){5,8}c/;
    shouldBe(re.exec("xaaaaaaaac"), ["xaaaaaaaac", "x", "a"], "Large min 5-8 matches 8");
    shouldBe(re.exec("xaaaac"), null, "Large min 5-8 fails with 4 a's");
    shouldBe(re.exec("xaaaaac"), ["xaaaaac", "x", "a"], "Large min 5-8 matches exactly 5");
})();

// Test 6: Stress test with iterations (reusing same regexp, MATCHING cases only)
(function() {
    var re = /(x){1,2}(a){2,5}b/;
    for (var i = 0; i < 100; i++) {
        var result = re.exec("xaaaaab");
        if (result[0] !== "xaaaaab" || result[1] !== "x" || result[2] !== "a")
            throw new Error("Stress test failed at iteration " + i);
    }
})();

// Test 7: Infinite max with non-zero min
(function() {
    var re = /(x){1,2}(a){2,}c/;
    shouldBe(re.exec("xaaaaac"), ["xaaaaac", "x", "a"], "{2,} matches all available");
    shouldBe(re.exec("xaac"), ["xaac", "x", "a"], "{2,} matches exactly min");
    shouldBe(re.exec("xac"), null, "{2,} fails below min");
})();

// Test 8: Edge cases
(function() {
    var re = /(x){1,2}(a){2,4}c/;
    shouldBe(re.exec(""), null, "Empty string fails");

    var re2 = /(x){1,2}(a){3,4}c/;
    shouldBe(re2.exec("xaaaac"), ["xaaaac", "x", "a"], "{3,4} matches 4");
    shouldBe(re2.exec("xaaac"), ["xaaac", "x", "a"], "{3,4} matches 3");
    shouldBe(re2.exec("xaac"), null, "{3,4} fails with 2");

    var re3 = /(x){1,2}(a){2,4}c/i;
    shouldBe(re3.exec("XaAaC"), ["XaAaC", "X", "a"], "Case insensitive");
})();

// Test 9: Simple patterns without trailing literal (no bug)
(function() {
    var re = /(a){2,4}/;
    shouldBe(re.exec("aaaa"), ["aaaa", "a"], "Split pattern greedy");
    shouldBe(re.exec(""), null, "0 chars fails min 2");
    shouldBe(re.exec("a"), null, "1 char fails min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "2 chars meets min 2");
    shouldBe(re.exec("aaa"), ["aaa", "a"], "3 chars exceeds min 2");
    shouldBe(re.exec("aaaaa"), ["aaaa", "a"], "5 chars limited to max 4");

    // Can do repeated non-match here since no trailing literal
    for (var i = 0; i < 100; i++) {
        re.exec("a");  // Below minimum
    }
})();

// Test 10: Simple non-greedy pattern
(function() {
    var re = /(a){2,4}?/;
    shouldBe(re.exec("aaaa"), ["aa", "a"], "Split pattern non-greedy");
    shouldBe(re.exec("a"), null, "1 char fails non-greedy min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "2 chars meets non-greedy min");
    shouldBe(re.exec("aaa"), ["aa", "a"], "3 chars - non-greedy stops at min");
})();

// Test 11: Anchored patterns (no bug with anchors)
(function() {
    var re = /^(a){2,4}$/;
    shouldBe(re.exec("aaa"), ["aaa", "a"], "Split pattern with anchors");
    shouldBe(re.exec("a"), null, "1 char fails anchored min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "2 chars meets anchored min");
    shouldBe(re.exec("aaaa"), ["aaaa", "a"], "4 chars at anchored max");
    shouldBe(re.exec("aaaaa"), null, "5 chars fails anchored max");

    // Can do repeated non-match with anchors
    for (var i = 0; i < 100; i++) {
        re.exec("a");
    }
})();

// Test 12: Larger minimum without trailing literal
(function() {
    var re = /(a){3,5}/;
    shouldBe(re.exec("aa"), null, "2 chars fails min 3");
    shouldBe(re.exec("aaa"), ["aaa", "a"], "3 chars meets min 3");
    shouldBe(re.exec("aaaa"), ["aaaa", "a"], "4 chars between min and max");
    shouldBe(re.exec("aaaaa"), ["aaaaa", "a"], "5 chars at max");
    shouldBe(re.exec("aaaaaa"), ["aaaaa", "a"], "6 chars limited to max 5");
})();

// Test 13: Two quantified groups without trailing literal
(function() {
    var re = /(x){1,2}(a){2,4}/;
    shouldBe(re.exec("xa"), null, "x + 1 a fails second group min");
    shouldBe(re.exec("xaa"), ["xaa", "x", "a"], "x + 2 a's succeeds");
    shouldBe(re.exec("xxaa"), ["xxaa", "x", "a"], "xx + 2 a's succeeds");
    shouldBe(re.exec("xxaaa"), ["xxaaa", "x", "a"], "xx + 3 a's succeeds");
})();

// Test 14: Non-capturing group minimum enforcement
(function() {
    var re = /(?:ab){2,4}/;
    shouldBe(re.exec("ab"), null, "1 ab fails min 2");
    shouldBe(re.exec("abab"), ["abab"], "2 ab's meets min");
    shouldBe(re.exec("ababab"), ["ababab"], "3 ab's between min and max");
    shouldBe(re.exec("abababab"), ["abababab"], "4 ab's at max");
    shouldBe(re.exec("ababababab"), ["abababab"], "5 ab's limited to max");
})();

// Test 15: Capture group correctness - capture should reflect last iteration
(function() {
    var re = /(ab){2,4}/;
    shouldBe(re.exec("abab"), ["abab", "ab"], "Capture is last 'ab' with 2 iterations");
    shouldBe(re.exec("ababab"), ["ababab", "ab"], "Capture is last 'ab' with 3 iterations");
    shouldBe(re.exec("abababab"), ["abababab", "ab"], "Capture is last 'ab' with 4 iterations");
})();

// Test 16: Nested capture groups
(function() {
    var re = /((a)b){2,4}/;
    shouldBe(re.exec("abab"), ["abab", "ab", "a"], "Nested captures correct");
    shouldBe(re.exec("ababab"), ["ababab", "ab", "a"], "Nested captures with 3 iterations");
})();

// Test 17: Alternation in quantified group
(function() {
    var re = /(a|b){2,4}/;
    shouldBe(re.exec("ab"), ["ab", "b"], "Alternation capture is last match");
    shouldBe(re.exec("ba"), ["ba", "a"], "Alternation capture is last match");
    shouldBe(re.exec("abab"), ["abab", "b"], "Alternation with 4 chars");
})();

// Test 18: Regression test for infinite loop bug in JIT backtracking
// Bug: When pattern `/(a){2,4}c/` failed to match "ac", the JIT would enter
// an infinite loop due to loading beginIndex instead of endIndex in BEGIN.bt.
// This test runs non-matching cases in a loop to catch any infinite loop.
(function() {
    var re = /(a){2,4}c/;
    // This specific case triggered the infinite loop: min not met + trailing literal
    for (var i = 0; i < 1000; i++) {
        if (re.exec("ac") !== null)
            throw new Error("Should not match 'ac' - only 1 'a' but min is 2");
    }
    // Also test other non-matching cases in loops
    for (var i = 0; i < 1000; i++) {
        if (re.exec("c") !== null)
            throw new Error("Should not match 'c' - no 'a' at all");
    }
    for (var i = 0; i < 1000; i++) {
        if (re.exec("ab") !== null)
            throw new Error("Should not match 'ab' - wrong trailing char");
    }
})();

// Test 19: Regression test for double-decrement bug in matchAmount
// Bug: matchAmount was decremented in both BEGIN.bt and END.bt, causing
// incorrect backtracking behavior. Test patterns that require precise
// iteration counting during backtracking.
(function() {
    // Pattern that requires backtracking into previous iteration
    var re = /(a+){2}b/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("aab");
        if (!result || result[0] !== "aab")
            throw new Error("(a+){2}b should match 'aab'");
    }
    // Non-matching case that exercises backtracking
    for (var i = 0; i < 1000; i++) {
        if (re.exec("ab") !== null)
            throw new Error("(a+){2}b should not match 'ab' - only 1 iteration possible");
    }
})();

// Test 20: Multi-alternative FixedCount backtracking
// Tests the simplified backtracking code for multi-alt FixedCount patterns
(function() {
    var re = /(?:aa|a){2}b/;
    // Matching cases
    for (var i = 0; i < 100; i++) {
        var result = re.exec("aab");
        if (!result || result[0] !== "aab")
            throw new Error("(?:aa|a){2}b should match 'aab'");
    }
    for (var i = 0; i < 100; i++) {
        var result = re.exec("aaab");
        if (!result || result[0] !== "aaab")
            throw new Error("(?:aa|a){2}b should match 'aaab'");
    }
    // Non-matching cases
    for (var i = 0; i < 100; i++) {
        if (re.exec("ab") !== null)
            throw new Error("(?:aa|a){2}b should not match 'ab'");
    }
})();

// Test 21: Content backtracking in FixedCount patterns
// Tests that content inside parentheses can be properly backtracked
(function() {
    // (a*b){2} needs to backtrack content (a*) to find valid split
    var re = /(a*b){2}/;
    for (var i = 0; i < 100; i++) {
        var result = re.exec("abab");
        if (!result || result[0] !== "abab")
            throw new Error("(a*b){2} should match 'abab'");
    }
    for (var i = 0; i < 100; i++) {
        var result = re.exec("aabab");
        if (!result || result[0] !== "aabab")
            throw new Error("(a*b){2} should match 'aabab'");
    }
    for (var i = 0; i < 100; i++) {
        var result = re.exec("aabaab");
        if (!result || result[0] !== "aabaab")
            throw new Error("(a*b){2} should match 'aabaab'");
    }
})();

// Test 22: FixedCount with exactly matching min==max (no split needed)
(function() {
    var re = /(a){3}b/;
    for (var i = 0; i < 100; i++) {
        var result = re.exec("aaab");
        if (!result || result[0] !== "aaab")
            throw new Error("(a){3}b should match 'aaab'");
    }
    for (var i = 0; i < 100; i++) {
        if (re.exec("aab") !== null)
            throw new Error("(a){3}b should not match 'aab'");
    }
    // Note: /(a){3}b/ on "aaaab" DOES match ("aaab" starting at position 1)
    for (var i = 0; i < 100; i++) {
        var result4 = re.exec("aaaab");
        if (!result4 || result4[0] !== "aaab")
            throw new Error("(a){3}b should match 'aaab' within 'aaaab'");
    }
})();

// Test 23: validatorjs IPv6 pattern fragment - capturing groups with {1,N}
// Multiple sibling capturing parens with non-zero min trigger the
// no-IR-expansion path in YarrPattern (m_hasCopiedParenSubexpressions guard);
// the JIT must handle min > 0 && min != max for capturing groups directly.
(function() {
    var re = /^(:(?:[0-9a-fA-F]{1,4})){1,7}$/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec(":a");
        if (!result || result[0] !== ":a") throw new Error("Test 23a iter " + i);
        result = re.exec(":a:b:c:d:e:f:7");
        if (!result || result[0] !== ":a:b:c:d:e:f:7") throw new Error("Test 23b iter " + i);
        if (re.exec(":") !== null) throw new Error("Test 23c iter " + i);
        if (re.exec("a") !== null) throw new Error("Test 23d iter " + i);
        if (re.exec(":a:b:c:d:e:f:g:h") !== null) throw new Error("Test 23e iter " + i);
    }
})();

// Test 24: Nested capturing min>0 inside outer min>0 (mirrors locale RFC 5646
// `(x(-[A-Za-z0-9]{1,8})+)`). Outer is FixedCount(1) with capturing inner that's
// (...){1,inf}. The inner BEGIN.bt count<min retry-via-content-backtrack path
// must not lose the outer capture.
(function() {
    var re = /^(x(-[A-Za-z0-9]{1,8})+)$/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("x-abc");
        if (!result || result[1] !== "x-abc" || result[2] !== "-abc")
            throw new Error("Test 24a iter " + i);
        result = re.exec("x-a-b-c");
        if (!result || result[1] !== "x-a-b-c" || result[2] !== "-c")
            throw new Error("Test 24b iter " + i);
        if (re.exec("x") !== null) throw new Error("Test 24c iter " + i);
        if (re.exec("-abc") !== null) throw new Error("Test 24d iter " + i);
    }
})();

// Test 25: Nested non-capturing min>0 - /(?:(?:ab)+){2,}/ requires the outer
// BEGIN.bt to backtrack INTO the prior iter's content (via END's content
// backtrack entry) rather than fail when count < min. With "abab", the outer
// needs to break iter 1 into a single (ab) so iter 2 can match.
(function() {
    var re = /^(?:(?:ab)+){2,}$/;
    for (var i = 0; i < 1000; i++) {
        if (!re.test("abab")) throw new Error("Test 25a iter " + i);
        if (!re.test("ababab")) throw new Error("Test 25b iter " + i);
        if (!re.test("abababab")) throw new Error("Test 25c iter " + i);
        if (re.test("ab")) throw new Error("Test 25d iter " + i);
        if (re.test("aba")) throw new Error("Test 25e iter " + i);
        if (re.test("ababa")) throw new Error("Test 25f iter " + i);
    }
})();

// Test 26: Non-greedy with capturing and min > 0
// /(((a){2})+){2,}?z/ exercises NonGreedy mandatory phase with deeply nested
// capturing parens — exposed by the yarr-jit-fixedcount-paren-context-free-on-skip suite.
(function() {
    var re = /(((a){2})+){2,}?z/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("aaaaz");
        if (!result || result[0] !== "aaaaz" || result[1] !== "aa" || result[2] !== "aa" || result[3] !== "a")
            throw new Error("Test 26a iter " + i);
        result = re.exec("aaaaaaz");
        if (!result || result[0] !== "aaaaaaz")
            throw new Error("Test 26b iter " + i);
        if (re.exec("aaz") !== null) throw new Error("Test 26c iter " + i);
        if (re.exec("z") !== null) throw new Error("Test 26d iter " + i);
    }
})();

// Test 27: + on capturing group (= {1,inf}) — ensure simple cases still work
(function() {
    var re = /^(a)+$/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("aaa");
        if (!result || result[0] !== "aaa" || result[1] !== "a")
            throw new Error("Test 27a iter " + i);
        if (re.exec("") !== null) throw new Error("Test 27b iter " + i);
    }
})();
