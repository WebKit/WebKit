//@ runDefault

// Tests for JIT compilation of non-capturing parenthesized subpatterns with fixed minimum counts.
// This tests the expansion of (?:x){n,m} where n > 1 into (?:x){1,1} × n + (?:x){0,m-n}.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected: " + expected + ", actual: " + actual);
}

function shouldBeTrue(value, message) {
    shouldBe(value, true, message);
}

function shouldBeFalse(value, message) {
    shouldBe(value, false, message);
}

// Basic tests for {3,} pattern (like marked's hr pattern)
(function testBasicThreeOrMore() {
    let re = /(?:a){3,}/;
    shouldBeTrue(re.test("aaa"), "{3,} should match 3");
    shouldBeTrue(re.test("aaaa"), "{3,} should match 4");
    shouldBeTrue(re.test("aaaaa"), "{3,} should match 5");
    shouldBeFalse(re.test("aa"), "{3,} should not match 2");
    shouldBeFalse(re.test("a"), "{3,} should not match 1");
    shouldBeFalse(re.test(""), "{3,} should not match empty");
})();

// Tests for {3,9} pattern (bounded range)
(function testBoundedRange() {
    let re = /^(?:a){3,9}$/;
    shouldBeFalse(re.test("aa"), "{3,9} should not match 2");
    shouldBeTrue(re.test("aaa"), "{3,9} should match 3");
    shouldBeTrue(re.test("aaaa"), "{3,9} should match 4");
    shouldBeTrue(re.test("aaaaaaaaa"), "{3,9} should match 9");
    shouldBeFalse(re.test("aaaaaaaaaa"), "{3,9} should not match 10");
})();

// Tests for {4,} pattern (at threshold)
(function testFourOrMore() {
    let re = /(?:ab){4,}/;
    shouldBeFalse(re.test("ababab"), "{4,} should not match 3 repetitions");
    shouldBeTrue(re.test("abababab"), "{4,} should match 4 repetitions");
    shouldBeTrue(re.test("ababababab"), "{4,} should match 5 repetitions");
})();

// Tests for {2,5} pattern
(function testTwoToFive() {
    let re = /^(?:xy){2,5}$/;
    shouldBeFalse(re.test("xy"), "{2,5} should not match 1");
    shouldBeTrue(re.test("xyxy"), "{2,5} should match 2");
    shouldBeTrue(re.test("xyxyxy"), "{2,5} should match 3");
    shouldBeTrue(re.test("xyxyxyxy"), "{2,5} should match 4");
    shouldBeTrue(re.test("xyxyxyxyxy"), "{2,5} should match 5");
    shouldBeFalse(re.test("xyxyxyxyxyxy"), "{2,5} should not match 6");
})();

// Tests for marked-style hr patterns
(function testMarkedHrPatterns() {
    // Simulating marked's hr patterns: (?:-[\t ]*){3,}
    let hrDash = /(?:-[\t ]*){3,}/;
    shouldBeTrue(hrDash.test("---"), "hr dash should match ---");
    shouldBeTrue(hrDash.test("- - -"), "hr dash should match - - -");
    shouldBeTrue(hrDash.test("-\t-\t-"), "hr dash should match with tabs");
    shouldBeTrue(hrDash.test("----"), "hr dash should match ----");
    shouldBeFalse(hrDash.test("--"), "hr dash should not match --");

    let hrStar = /(?:\*[\t ]*){3,}/;
    shouldBeTrue(hrStar.test("***"), "hr star should match ***");
    shouldBeTrue(hrStar.test("* * *"), "hr star should match * * *");
    shouldBeFalse(hrStar.test("**"), "hr star should not match **");

    let hrUnderscore = /(?:_[\t ]*){3,}/;
    shouldBeTrue(hrUnderscore.test("___"), "hr underscore should match ___");
    shouldBeTrue(hrUnderscore.test("_ _ _"), "hr underscore should match _ _ _");
    shouldBeFalse(hrUnderscore.test("__"), "hr underscore should not match __");
})();

// Tests for complex patterns inside parentheses
(function testComplexPatterns() {
    let re = /(?:[a-z]+\d+){3,}/;
    shouldBeTrue(re.test("a1b2c3"), "complex pattern should match");
    shouldBeTrue(re.test("abc123def456ghi789"), "complex pattern should match longer");
    shouldBeFalse(re.test("a1b2"), "complex pattern should not match 2 repetitions");
})();

// Tests for nested parentheses
(function testNestedParentheses() {
    let re = /(?:(?:ab)+){2,}/;
    shouldBeTrue(re.test("abab"), "nested should match abab");
    shouldBeTrue(re.test("ababab"), "nested should match ababab");
    shouldBeTrue(re.test("abababab"), "nested should match abababab");
})();

// Ensure capturing groups still work correctly (should NOT be unrolled)
(function testCapturingGroups() {
    let re = /(a){3}/;
    let match = "aaa".match(re);
    shouldBe(match[0], "aaa", "capturing group full match");
    shouldBe(match[1], "a", "capturing group should capture last iteration");

    let re2 = /(ab){2,4}/;
    let match2 = "abababab".match(re2);
    shouldBe(match2[0], "abababab", "capturing group {2,4} full match");
    shouldBe(match2[1], "ab", "capturing group {2,4} should capture last iteration");
})();

// Tests for {n,n} fixed count (exact match)
(function testExactCount() {
    let re = /^(?:abc){3,3}$/;
    shouldBeFalse(re.test("abcabc"), "{3,3} should not match 2");
    shouldBeTrue(re.test("abcabcabc"), "{3,3} should match exactly 3");
    shouldBeFalse(re.test("abcabcabcabc"), "{3,3} should not match 4");
})();

// Performance-sensitive test: run many times to ensure JIT compilation
(function testJITCompilation() {
    let re = /(?:-[\t ]*){3,}/;
    let testString = "---";
    for (let i = 0; i < 10000; i++) {
        if (!re.test(testString))
            throw new Error("JIT test failed at iteration " + i);
    }
})();

// Tests for greedy vs non-greedy (should both work)
(function testGreedyNonGreedy() {
    // Greedy (default)
    let greedy = /(?:a){2,4}/;
    let greedyMatch = "aaaa".match(greedy);
    shouldBe(greedyMatch[0], "aaaa", "greedy should match all 4");

    // Non-greedy
    let nonGreedy = /(?:a){2,4}?/;
    let nonGreedyMatch = "aaaa".match(nonGreedy);
    shouldBe(nonGreedyMatch[0], "aa", "non-greedy should match minimum 2");
})();

// Edge case: {1,n} should use original path (no unrolling needed)
(function testOneToN() {
    let re = /(?:x){1,5}/;
    shouldBeTrue(re.test("x"), "{1,5} should match 1");
    shouldBeTrue(re.test("xxxxx"), "{1,5} should match 5");
})();
