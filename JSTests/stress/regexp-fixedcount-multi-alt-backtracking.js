//@ runDefault

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Test multi-alternative FixedCount patterns that require inter-iteration backtracking.
// These tests verify that when all alternatives fail in a later iteration,
// the JIT correctly backtracks to try different alternatives in earlier iterations.

// Test 1: Two alternatives, need to backtrack greedy first alt
(function() {
    var re = /(?:aa|a){2}b/;
    var result = re.exec("aab");
    // Naive: "aa" in iter 1 leaves no 'a' for iter 2
    // Correct: "a" in iter 1, "a" in iter 2, then "b"
    shouldBe(result, ["aab"], "Two alternatives with backtracking");
})();

// Test 2: Three alternatives, basic matching
(function() {
    var re = /(?:aaa|aa|a){3}b/;
    var result = re.exec("aaab");
    // Must use "a" three times
    shouldBe(result, ["aaab"], "Three alternatives, 3 iterations");
})();

// Test 3: Three alternatives with more complex input
(function() {
    var re = /(?:aaa|aa|a){4}b/;
    var result = re.exec("aaaab");
    // Options: "aaa"+"a", "aa"+"aa", "aa"+"a"+"a", "a"+"aaa", "a"+"aa"+"a", "a"+"a"+"aa", "a"+"a"+"a"+"a"
    shouldBe(result, ["aaaab"], "Three alternatives, 4 iterations");
})();

// Test 4: Alternatives with different patterns
(function() {
    var re = /(?:ab|a){2}c/;
    var result = re.exec("aac");
    // "ab" can't match at pos 0 (input is "aac"), so use "a" twice
    shouldBe(result, ["aac"], "Mixed alternatives (ab|a)");
})();

// Test 5: Four alternatives simple
(function() {
    var re = /(?:a|b|c|d){4}e/;
    var result = re.exec("abcde");
    shouldBe(result, ["abcde"], "Four single-char alternatives");
})();

// Test 6: Three alternatives no match
(function() {
    var re = /(?:aaa|aa|a){3}c/;
    var result = re.exec("aaab");
    shouldBe(result, null, "Three alternatives no match");
})();

// Test 7: Capturing with multiple alternatives
(function() {
    var re = /(aa|a){2}b/;
    var result = re.exec("aaab");
    // Iter 1: "aa" matches, iter 2: needs "a", backtrack iter 1 to "a"
    // Iter 1: "a", iter 2: "a" matches, then "b"
    shouldBe(result, ["aaab", "a"], "Capturing with backtracking");
})();

// Test 8: Non-capturing, alternatives with complex patterns
(function() {
    var re = /(?:xy|x){2}z/;
    var result = re.exec("xxz");
    shouldBe(result, ["xxz"], "Non-capturing xy|x backtracking");
})();

// Test 9: Many iterations with two alternatives
(function() {
    var re = /(?:ab|a){5}c/;
    var result = re.exec("aaaaac");
    shouldBe(result, ["aaaaac"], "Five iterations two alternatives");
})();

// Test 10: Edge case - exactly right number of characters
(function() {
    var re = /(?:aaa|aa|a){3}$/;
    var result = re.exec("aaa");
    shouldBe(result, ["aaa"], "Exact character count");
})();

// Test 11: Complex pattern with word boundaries
(function() {
    var re = /(?:the|a){2}/;
    var result = re.exec("thea");
    shouldBe(result, ["thea"], "Word patterns");
})();

// Test 12: Alternatives with overlapping prefixes
(function() {
    var re = /(?:abc|ab|a){2}d/;
    var result = re.exec("abd");
    // "ab" + nothing doesn't work, need "a" + "b" doesn't work
    // This should fail because "bd" can't match "abc", "ab", or "a" after first iteration
    shouldBe(result, null, "Overlapping prefixes no match");
})();

// Test 13: Alternatives with overlapping prefixes - match
(function() {
    var re = /(?:abc|ab|a){2}d/;
    var result = re.exec("aad");
    shouldBe(result, ["aad"], "Overlapping prefixes match");
})();

// Test 14: Multi-alt with backtrackable content inside (greedy quantifier)
(function() {
    var re = /(a+|b){2}c/;
    var result = re.exec("aac");
    // "a+" greedily matches "aa" in iter 1, iter 2 needs something, backtrack
    shouldBe(result, ["aac", "a"], "Multi-alt with greedy content");
})();

// Test 15: Multi-alt with backtrackable content - different alternatives
(function() {
    var re = /(a+|bb){2}c/;
    var result = re.exec("abbc");
    // Iter 1: "a+" matches "a", iter 2: "bb" matches
    shouldBe(result, ["abbc", "bb"], "Multi-alt with greedy, different alts");
})();

// Test 16: Multi-alt with non-greedy content inside
(function() {
    var re = /(a+?|b){2}c/;
    var result = re.exec("aac");
    shouldBe(result, ["aac", "a"], "Multi-alt with non-greedy content");
})();

// Test 17: Multi-alt capturing with 3 alternatives and backtrackable content
(function() {
    var re = /(a+|bb|c){3}d/;
    var result = re.exec("abcd");
    // Iter 1: "a+", iter 2: "b" fails, tries "bb" fails, tries "c" fails
    // Backtrack iter 1: would need to try fewer 'a's but there's only 1
    // Actually: a, b, c don't work... let's use different input
    var result2 = re.exec("aabbcd");
    shouldBe(result2, ["aabbcd", "c"], "Multi-alt 3 alternatives with greedy");
})();

// Test 18: Large iteration count
(function() {
    var re = /(?:aa|a){10}b/;
    var result = re.exec("aaaaaaaaaab");
    // 10 iterations needed, 10 'a's available
    shouldBe(result, ["aaaaaaaaaab"], "Large iteration count");
})();

// Test 19: Nested FixedCount patterns
(function() {
    var re = /((?:a|b){2}){2}c/;
    var result = re.exec("ababc");
    shouldBe(result, ["ababc", "ab"], "Nested FixedCount");
})();

// Test 20: Character class inside multi-alt FixedCount
(function() {
    var re = /([a-z]+|[0-9]){2}x/;
    var result = re.exec("ab3x");
    shouldBe(result, ["ab3x", "3"], "Character class in multi-alt");
})();

// Test 21: Empty string handling
(function() {
    var re = /(?:a|){2}b/;
    var result = re.exec("b");
    shouldBe(result, ["b"], "Empty alternative");
})();

// Test 22: Unicode patterns (if supported)
(function() {
    var re = /(?:ab|a){2}c/;
    var result = re.exec("aac");
    shouldBe(result, ["aac"], "Basic pattern before unicode test");
})();

// Test 23: Multi-alt with backreference inside
(function() {
    var re = /(([a-c])\2|x){2}y/;
    var result = re.exec("aaxxy");
    // "aa" at start doesn't lead to match, pattern finds "xxy" starting at pos 2
    shouldBe(result, ["xxy", "x", null], "Multi-alt with backreference");
})();

// Test 24: Deeply nested alternatives
(function() {
    var re = /((?:a|b)|(?:c|d)){4}e/;
    var result = re.exec("abcde");
    shouldBe(result, ["abcde", "d"], "Deeply nested alternatives");
})();

// Test 25: Multi-alt with zero-width assertions
(function() {
    var re = /(?:a(?=b)|a){2}b/;
    var result = re.exec("aab");
    shouldBe(result, ["aab"], "Multi-alt with lookahead");
})();

// Test 26: Deeply nested FixedCount - stresses freelist reuse across nesting levels
(function() {
    // Inner {2} and outer {2} both use ParenContext, sharing the same freelist
    var re = /((a){2}){2}b/;
    var result = re.exec("aaaab");
    shouldBe(result, ["aaaab", "aa", "a"], "Deeply nested FixedCount with captures");
})();

// Test 27: Triple nested FixedCount
(function() {
    var re = /(((a){2}){2}){2}b/;
    var result = re.exec("aaaaaaaab");
    // Captures reflect the LAST value matched by each group
    shouldBe(result, ["aaaaaaaab", "aaaa", "aa", "a"], "Triple nested FixedCount");
})();

// Test 28: Nested FixedCount with backtracking at inner level
(function() {
    // Inner (a+){2} needs backtracking, outer {2} wraps it
    var re = /((a+){2}){2}b/;
    var result = re.exec("aaaaaab");
    // Need 4 groups of a+ that sum to 6
    // Outer capture = last outer iteration's match, inner capture = last inner iteration's match
    shouldBe(result, ["aaaaaab", "aa", "a"], "Nested FixedCount with inner backtracking");
})();

// Test 29: Nested multi-alt FixedCount
(function() {
    var re = /((?:aa|a){2}){2}c/;
    var result = re.exec("aaaac");
    // Inner needs to match 2 times, outer needs 2 times = 4 a's total
    shouldBe(result, ["aaaac", "aa"], "Nested multi-alt FixedCount");
})();

// Test 30: Stress many iterations - allocates many ParenContexts
(function() {
    var re = /(?:aa|a){20}b/;
    var result = re.exec("aaaaaaaaaaaaaaaaaaaab");
    // 20 a's, need 20 iterations
    shouldBe(result, ["aaaaaaaaaaaaaaaaaaaab"], "20 iterations stress test");
})();

// Test 31: Stress with capturing - verifies capture restoration on backtrack
(function() {
    var re = /(aa|a){20}b/;
    var result = re.exec("aaaaaaaaaaaaaaaaaaaab");
    shouldBe(result, ["aaaaaaaaaaaaaaaaaaaab", "a"], "20 iterations with capture");
})();

// Test 32: Many iterations with forced backtracking
(function() {
    // Each iteration can match 1-3 a's, total 15 a's
    // Forces many backtrack scenarios
    var re = /(?:aaa|aa|a){10}b/;
    var result = re.exec("aaaaaaaaaaaaaaab");
    shouldBe(result, ["aaaaaaaaaaaaaaab"], "10 iterations with 3 alternatives backtracking");
})();

// Test 33: Interleaved nested FixedCount with different quantifiers
(function() {
    var re = /((?:a){3}(?:b){2}){2}c/;
    var result = re.exec("aaabbaaabbc");
    shouldBe(result, ["aaabbaaabbc", "aaabb"], "Interleaved nested FixedCount");
})();

// Test 34: Complex nested with backtrackable content
(function() {
    var re = /((a+b){2}){2}c/;
    // Use inputs that match cleanly without excessive backtracking
    var result = re.exec("ababababc");
    shouldBe(result, ["ababababc", "abab", "ab"], "Complex nested - match case");

    // Test with slightly different input (finds match within)
    var result2 = re.exec("aababababc");
    shouldBe(result2, ["aababababc", "abab", "ab"], "Complex nested - variant grouping");
})();

// Test 35: FixedCount with alternation where first alt always fails
(function() {
    var re = /(?:xyz|a){5}b/;
    var result = re.exec("aaaaab");
    shouldBe(result, ["aaaaab"], "First alt always fails");
})();

// Test 36: FixedCount with alternation where second alt rarely matches
(function() {
    var re = /(?:a|xyz){5}b/;
    var result = re.exec("aaaaab");
    shouldBe(result, ["aaaaab"], "Second alt rarely matches");
})();

// Test 37: Nested with multiple capture groups
(function() {
    var re = /((a)(b)){2}c/;
    var result = re.exec("ababc");
    shouldBe(result, ["ababc", "ab", "a", "b"], "Nested with multiple captures");
})();

// Test 38: Very deep nesting (4 levels)
(function() {
    var re = /((((a){2}){2}){2}){2}b/;
    var result = re.exec("aaaaaaaaaaaaaaaab");
    // 2^4 = 16 a's
    shouldBe(result, ["aaaaaaaaaaaaaaaab", "aaaaaaaa", "aaaa", "aa", "a"], "4-level nested FixedCount");
})();

// Test 39: Mixed greedy and non-greedy in nested FixedCount
(function() {
    var re = /((a+?){2}){2}b/;
    var result = re.exec("aaaab");
    // Non-greedy a+? matches 1 'a' each time
    shouldBe(result, ["aaaab", "aa", "a"], "Mixed non-greedy nested FixedCount");
})();

// Test 40: FixedCount with optional groups inside
(function() {
    var re = /((?:a)?b){3}c/;
    var result = re.exec("abbbc");
    shouldBe(result, ["abbbc", "b"], "Optional inside FixedCount");

    var result2 = re.exec("ababbc");
    shouldBe(result2, ["ababbc", "b"], "Optional inside FixedCount 2");
})();

// Test 41: Stress test - rapid alloc/free cycles
(function() {
    // This pattern forces many incomplete contexts to be allocated and freed
    var re = /((?:aaa|aa|a){3}x|(?:aaa|aa|a){3}){2}y/;
    var result = re.exec("aaaaaay");
    shouldBe(result, ["aaaaaay", "aaa"], "Rapid alloc/free stress");
})();

// Test 42: Alternation with empty match possibility
(function() {
    var re = /(?:a|){10}b/;
    var result = re.exec("aaab");
    shouldBe(result, ["aaab"], "Empty match in alternation");

    var result2 = re.exec("b");
    shouldBe(result2, ["b"], "All empty matches");
})();

// Test 43: Nested FixedCount with alternation at multiple levels
(function() {
    var re = /((?:ab|a){2}|(?:ba|b){2}){2}c/;
    var result = re.exec("aaaabbc");
    // Match found at different position in input
    shouldBe(result, ["aabbc", "bb"], "Multi-level alternation nesting");
})();

// Test 44: Large iteration count with captures - memory pressure
(function() {
    var re = /(a){50}b/;
    var result = re.exec("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab");
    shouldBe(result, ["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", "a"], "50 iterations with capture");
})();

// Test 45: Nested with different capture counts
(function() {
    var re = /((a)(b)(c)){2}d/;
    var result = re.exec("abcabcd");
    shouldBe(result, ["abcabcd", "abc", "a", "b", "c"], "Multiple captures in nested");
})();

// Test 46: Backtracking that exhausts all contexts then succeeds
(function() {
    var re = /(?:aaaa|aaa|aa|a){4}$/;
    var result = re.exec("aaaa");
    // Only way to match: "a" + "a" + "a" + "a"
    shouldBe(result, ["aaaa"], "Exhaust all alternatives then succeed");
})();

// Test 47: Multiple FixedCount groups in sequence (not nested)
(function() {
    var re = /(a){2}(b){3}(c){2}d/;
    var result = re.exec("aabbbccd");
    shouldBe(result, ["aabbbccd", "a", "b", "c"], "Sequential FixedCount groups");
})();

// Test 48: FixedCount with character classes and alternation
(function() {
    var re = /([a-c]+|[x-z]){3}!/;
    var result = re.exec("abxyz!");
    // Match starts at position 2 (xyz!)
    shouldBe(result, ["xyz!", "z"], "Character classes in alternation");
})();

// Test 49: Very long alternation chain in FixedCount
(function() {
    var re = /(?:a|b|c|d|e|f|g|h){8}i/;
    var result = re.exec("abcdefghi");
    shouldBe(result, ["abcdefghi"], "8 alternatives, 8 iterations");
})();

// Test 50: Nested FixedCount with backreference
(function() {
    var re = /((.)\2){2}x/;
    var result = re.exec("aabbx");
    shouldBe(result, ["aabbx", "bb", "b"], "Nested FixedCount with backreference");
})();

// Test 51: Stress - many levels with small counts
(function() {
    var re = /(((((a){1}){1}){1}){1}){5}b/;
    var result = re.exec("aaaaab");
    shouldBe(result, ["aaaaab", "a", "a", "a", "a", "a"], "5-level single-count nesting");
})();

// Test 52: FixedCount in lookahead context
(function() {
    var re = /(?=((a){3}))aaa/;
    var result = re.exec("aaab");
    shouldBe(result, ["aaa", "aaa", "a"], "FixedCount in lookahead");
})();

// Test 53: Multiple named captures in FixedCount
(function() {
    var re = /(?<letter>(?:aa|a)){3}b/;
    var result = re.exec("aaab");
    shouldBe(result.groups.letter, "a", "Named capture in FixedCount");
    shouldBe(result[0], "aaab", "Named capture full match");
})();

// Test 54: Nested FixedCount with named groups
(function() {
    var re = /(?<outer>(?<inner>a){2}){2}b/;
    var result = re.exec("aaaab");
    shouldBe(result.groups.outer, "aa", "Named outer group");
    shouldBe(result.groups.inner, "a", "Named inner group");
})();

// Test 55: FixedCount stress with alternation and captures
(function() {
    for (var i = 1; i <= 10; i++) {
        var input = "a".repeat(i) + "b";
        var re = new RegExp("(a){" + i + "}b");
        var result = re.exec(input);
        shouldBe(result !== null, true, "Dynamic FixedCount " + i);
        shouldBe(result[0], input, "Dynamic FixedCount " + i + " full match");
    }
})();

// Test 56: Stress nested with dynamic counts
(function() {
    var re = /((a){3}){3}b/;
    var result = re.exec("aaaaaaaaab");
    shouldBe(result, ["aaaaaaaaab", "aaa", "a"], "3x3 nested FixedCount");
})();

// Test 57: Interleaved captures and non-captures
(function() {
    var re = /((?:a)(b)(?:c)(d)){2}e/;
    var result = re.exec("abcdabcde");
    shouldBe(result, ["abcdabcde", "abcd", "b", "d"], "Interleaved capture and non-capture");
})();

// Test 58: Multiple FixedCount with shared alternatives
(function() {
    var re = /(?:aa|a){2}(?:bb|b){2}c/;
    var result = re.exec("aabbc");
    shouldBe(result, ["aabbc"], "Multiple FixedCount sequence");

    var result2 = re.exec("aaabbc");
    shouldBe(result2, ["aaabbc"], "Multiple FixedCount sequence 2");
})();

// Test 59: Verify capture groups are correctly restored on backtrack
(function() {
    var re = /(aa|a)(bb|b){2}c/;
    var result = re.exec("abbbc");
    // First group "a", second group matches "bb" then needs "b", backtracks
    shouldBe(result, ["abbbc", "a", "b"], "Capture restoration on backtrack");
})();

// Test 60: Final stress - everything combined
(function() {
    var re = /(((?:aaa|aa|a){2})((?:bbb|bb|b){2})){2}c/;
    var result = re.exec("aaabbaaabbc");
    // Outer captures last iteration, inner captures show nested group values
    shouldBe(result, ["aaabbaaabbc", "aaabb", "aaa", "bb"], "Ultimate stress test");
})();
