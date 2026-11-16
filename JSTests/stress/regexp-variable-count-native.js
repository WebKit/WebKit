//@ runDefault

// Tests for native JIT support of variable counted parentheses with non-zero minimum.
// These patterns are now handled natively by the JIT using FixedCount-style content
// backtracking, without splitting into FixedCount{min} + Greedy{0,max-min}.

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// ==== Test 1: Basic greedy pattern with non-zero min ====
(function() {
    var re = /(a){2,4}/;
    shouldBe(re.exec("a"), null, "Single 'a' fails min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "Two 'a's matches min 2");
    shouldBe(re.exec("aaa"), ["aaa", "a"], "Three 'a's matches");
    shouldBe(re.exec("aaaa"), ["aaaa", "a"], "Four 'a's matches max 4");
    shouldBe(re.exec("aaaaa"), ["aaaa", "a"], "Five 'a's matches max 4 (greedy)");
    shouldBe(re.exec(""), null, "Empty string fails");
})();

// ==== Test 2: Basic non-greedy pattern with non-zero min ====
(function() {
    var re = /(a){2,4}?/;
    shouldBe(re.exec("a"), null, "Single 'a' fails min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "Two 'a's matches min 2 (non-greedy stops at min)");
    shouldBe(re.exec("aaa"), ["aa", "a"], "Three 'a's matches min 2 (non-greedy)");
    shouldBe(re.exec("aaaa"), ["aa", "a"], "Four 'a's matches min 2 (non-greedy)");
})();

// ==== Test 3: Greedy with trailing literal (forces backtracking) ====
(function() {
    var re = /(a){2,4}b/;
    shouldBe(re.exec("ab"), null, "One 'a' fails min 2");
    shouldBe(re.exec("aab"), ["aab", "a"], "Two 'a's matches");
    shouldBe(re.exec("aaab"), ["aaab", "a"], "Three 'a's matches");
    shouldBe(re.exec("aaaab"), ["aaaab", "a"], "Four 'a's matches max");
    shouldBe(re.exec("aaaaab"), ["aaaab", "a"], "Five 'a's - matches 4 from position 1");
})();

// ==== Test 4: Non-greedy with trailing literal ====
(function() {
    var re = /(a){2,4}?b/;
    shouldBe(re.exec("ab"), null, "One 'a' fails min 2");
    shouldBe(re.exec("aab"), ["aab", "a"], "Two 'a's matches min");
    shouldBe(re.exec("aaab"), ["aaab", "a"], "Three 'a's - non-greedy forced to match 3");
    shouldBe(re.exec("aaaab"), ["aaaab", "a"], "Four 'a's - non-greedy forced to match 4");
    shouldBe(re.exec("aaaaab"), ["aaaab", "a"], "Five 'a's - matches 4 from position 1");
})();

// ==== Test 5: Non-capturing group with non-zero min ====
(function() {
    var re = /(?:ab){2,4}c/;
    shouldBe(re.exec("abc"), null, "One 'ab' fails min 2");
    shouldBe(re.exec("ababc"), ["ababc"], "Two 'ab' matches min");
    shouldBe(re.exec("abababc"), ["abababc"], "Three 'ab' matches");
    shouldBe(re.exec("ababababc"), ["ababababc"], "Four 'ab' matches max");
    shouldBe(re.exec("abababababc"), ["ababababc"], "Five 'ab' - matches 4 from position 2");
})();

// ==== Test 6: Infinite max with non-zero min ====
(function() {
    var re = /(a){2,}/;
    shouldBe(re.exec("a"), null, "One 'a' fails min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "Two 'a's matches min");
    shouldBe(re.exec("aaaaaaaaa"), ["aaaaaaaaa", "a"], "Many 'a's matches (greedy)");
})();

// ==== Test 7: Infinite max non-greedy ====
(function() {
    var re = /(a){2,}?/;
    shouldBe(re.exec("a"), null, "One 'a' fails min 2");
    shouldBe(re.exec("aa"), ["aa", "a"], "Two 'a's matches min");
    shouldBe(re.exec("aaaaaaaaa"), ["aa", "a"], "Many 'a's matches min (non-greedy)");
})();

// ==== Test 8: Larger min values ====
(function() {
    var re = /(x){5,8}/;
    shouldBe(re.exec("xxxx"), null, "4 chars fails min 5");
    shouldBe(re.exec("xxxxx"), ["xxxxx", "x"], "5 chars matches min");
    shouldBe(re.exec("xxxxxxxx"), ["xxxxxxxx", "x"], "8 chars matches max");
    shouldBe(re.exec("xxxxxxxxx"), ["xxxxxxxx", "x"], "9 chars matches max 8 (greedy)");
})();

// ==== Test 9: Nested capturing groups ====
(function() {
    var re = /((a)(b)){2,4}/;
    var result = re.exec("ababab");
    shouldBe(result[0], "ababab", "Full match");
    shouldBe(result[1], "ab", "Outer capture (last iteration)");
    shouldBe(result[2], "a", "Inner capture 1 (last iteration)");
    shouldBe(result[3], "b", "Inner capture 2 (last iteration)");
})();

// ==== Test 10: Alternation inside quantified group ====
(function() {
    var re = /(a|b){2,4}/;
    shouldBe(re.exec("a"), null, "One char fails min 2");
    shouldBe(re.exec("ab"), ["ab", "b"], "Two chars matches");
    shouldBe(re.exec("ba"), ["ba", "a"], "Two chars matches (other order)");
    shouldBe(re.exec("abab"), ["abab", "b"], "Four chars matches");
    shouldBe(re.exec("aaaaa"), ["aaaa", "a"], "Five chars matches max 4");
})();

// ==== Test 11: Greedy inner content - content backtracking ====
(function() {
    var re = /(a+){2}b/;
    shouldBe(re.exec("ab"), null, "One iteration fails");
    shouldBe(re.exec("aab"), ["aab", "a"], "Two iterations matches");
    shouldBe(re.exec("aaab"), ["aaab", "a"], "With extra chars in iteration");
})();

// ==== Test 12: Multiple quantified groups ====
(function() {
    var re = /(x){1,2}(a){2,4}c/;
    shouldBe(re.exec("xaac"), ["xaac", "x", "a"], "Min matches for both");
    shouldBe(re.exec("xxaaaac"), ["xxaaaac", "x", "a"], "Max for both");
    shouldBe(re.exec("xac"), null, "Second group fails min");
})();

// ==== Test 13: Case insensitive ====
(function() {
    var re = /(a){2,4}/i;
    shouldBe(re.exec("aA"), ["aA", "A"], "Mixed case matches");
    shouldBe(re.exec("AaAa"), ["AaAa", "a"], "Four mixed case chars");
})();

// ==== Test 14: Unicode ====
(function() {
    var re = /(\u{1F600}){2,4}/u;
    shouldBe(re.exec("\u{1F600}"), null, "One emoji fails min 2");
    shouldBe(re.exec("\u{1F600}\u{1F600}"), ["\u{1F600}\u{1F600}", "\u{1F600}"], "Two emojis match");
})();

// ==== Test 15: Stress test - Greedy ====
(function() {
    var re = /(a){2,100}/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("aaaaaaaaaa"); // 10 a's
        if (result[0] !== "aaaaaaaaaa" || result[1] !== "a")
            throw new Error("Stress test 1 failed at iteration " + i);
    }
})();

// ==== Test 16: Stress test - NonGreedy ====
(function() {
    var re = /(a){2,100}?/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("aaaaaaaaaa"); // 10 a's
        if (result[0] !== "aa" || result[1] !== "a")
            throw new Error("Stress test 2 failed at iteration " + i);
    }
})();

// ==== Test 17: Stress test - backtracking ====
(function() {
    var re = /(a){2,5}b/;
    for (var i = 0; i < 1000; i++) {
        var result = re.exec("aaaab");
        if (result[0] !== "aaaab" || result[1] !== "a")
            throw new Error("Stress test 3 failed at iteration " + i);
    }
})();

// ==== Test 18: Edge case - min equals max minus 1 ====
(function() {
    var re = /(a){3,4}/;
    shouldBe(re.exec("aa"), null, "Two chars fails min 3");
    shouldBe(re.exec("aaa"), ["aaa", "a"], "Three chars matches");
    shouldBe(re.exec("aaaa"), ["aaaa", "a"], "Four chars matches max");
    shouldBe(re.exec("aaaaa"), ["aaaa", "a"], "Five chars matches max 4");
})();

// ==== Test 19: Non-greedy forcing backtrack ====
(function() {
    var re = /(a){2,5}?aaab/;
    shouldBe(re.exec("aaaaaab"), ["aaaaaab", "a"], "Non-greedy forced to match 3 a's");
    shouldBe(re.exec("aaaab"), null, "Too short");
    shouldBe(re.exec("aaaaab"), ["aaaaab", "a"], "Exact minimum");
})();

// ==== Test 20: Alternation patterns - the original motivating case ====
(function() {
    var re = /(aa|aaaa){2,3}$/;
    shouldBe(re.exec("aaaaaa"), ["aaaaaa", "aa"], "6 a's: 3x'aa'");
    shouldBe(re.exec("aaaaaaaa"), ["aaaaaaaa", "aaaa"], "8 a's: 2x'aaaa'");
    shouldBe(re.exec("aaaa"), ["aaaa", "aa"], "4 a's: 2x'aa'");
    shouldBe(re.exec("aaa"), null, "3 a's: fails");
})();

// ==== Test 21: Alternation patterns requiring specific combinations ====
(function() {
    var re = /(a|aaa){2}x/;
    shouldBe(re.exec("aaaax"), ["aaaax", "aaa"], "4 a's + x: 'a'+'aaa'");
    shouldBe(re.exec("aax"), ["aax", "a"], "2 a's + x: 'a'+'a'");
    shouldBe(re.exec("ax"), null, "1 a + x: fails");
})();

// ==== Test 22: Greedy content backtracking with min > 0 ====
(function() {
    var re = /(a+){2,4}b/;
    shouldBe(re.exec("aaab"), ["aaab", "a"], "a+ must split to allow 2 iterations");
    shouldBe(re.exec("aab"), ["aab", "a"], "2 a's: 'a'+'a'+b");
    shouldBe(re.exec("ab"), null, "1 a: fails min 2");
    shouldBe(re.exec("aaaaab"), ["aaaaab", "a"], "5 a's: 4 iterations of a+");
})();

// ==== Test 23: Content backtracking with fixed count ====
(function() {
    var re = /(a+){2}b/;
    shouldBe(re.exec("aaab"), ["aaab", "a"], "3 a's: need to backtrack a+ to allow 2 iters");
    shouldBe(re.exec("aab"), ["aab", "a"], "2 a's: exactly 2 iters");
    shouldBe(re.exec("ab"), null, "1 a: fails");
})();

// ==== Test 24: Content backtracking with fixed count (3) ====
(function() {
    var re = /(a+){3}b/;
    shouldBe(re.exec("aaab"), ["aaab", "a"], "3 a's: exactly 3 iters of 1 a each");
    shouldBe(re.exec("aaaab"), ["aaaab", "a"], "4 a's: need to redistribute");
    shouldBe(re.exec("aab"), null, "2 a's: can't make 3 iters");
})();

// ==== Test 25: NonGreedy with content backtracking ====
(function() {
    var re = /(a+){2,4}?b/;
    shouldBe(re.exec("aaab"), ["aaab", "a"], "3 a's + b");
    shouldBe(re.exec("aab"), ["aab", "a"], "2 a's + b");
    shouldBe(re.exec("ab"), null, "1 a: fails min 2");
})();

// ==== Test 26: Multi-alternative with content backtracking ====
(function() {
    var re = /(a+|b+){2,4}c/;
    shouldBe(re.exec("aabbc"), ["aabbc", "bb"], "Mixed alternatives");
    shouldBe(re.exec("abc"), ["abc", "b"], "Single chars");
    shouldBe(re.exec("ac"), null, "Only 1 iteration: fails");
})();

// ==== Test 27: Single-char alternatives ====
(function() {
    var re = /(a|b|c){2,3}d/;
    shouldBe(re.exec("abcd"), ["abcd", "c"], "3 iterations");
    shouldBe(re.exec("abd"), ["abd", "b"], "2 iterations");
    shouldBe(re.exec("ad"), null, "1 iteration: fails");
})();

// ==== Test 28: Acceptance at different counts ====
(function() {
    var re = /(abc){2,4}abcd/;
    // 3 iterations + "abcd" suffix requires accepting 3 iterations (not 4)
    shouldBe(re.exec("abcabcabcabcd"), ["abcabcabcabcd", "abc"], "Accept 3 iters + abcd");
    shouldBe(re.exec("abcabcabcd"), ["abcabcabcd", "abc"], "Accept 2 iters + abcd");
    shouldBe(re.exec("abcabcd"), null, "1 iter + abcd: fails min 2");
})();

// ==== Test 29: Accept 2 iterations with suffix ====
(function() {
    var re = /(ab){2,3}abc/;
    shouldBe(re.exec("abababc"), ["abababc", "ab"], "2 iters of (ab) + abc suffix");
    shouldBe(re.exec("ababababc"), ["ababababc", "ab"], "3 iters of (ab) + abc suffix");
    shouldBe(re.exec("ababc"), null, "Too short: 1 iter + abc doesn't meet min 2");
})();

// ==== Test 30: Mixed fixed and variable count ====
(function() {
    var re = /(a){3}(b){2,4}c/;
    shouldBe(re.exec("aaabbc"), ["aaabbc", "a", "b"], "Fixed 3, var 2");
    shouldBe(re.exec("aaabbbc"), ["aaabbbc", "a", "b"], "Fixed 3, var 3");
    shouldBe(re.exec("aaabbbbc"), ["aaabbbbc", "a", "b"], "Fixed 3, var 4");
    shouldBe(re.exec("aabbc"), null, "Fails fixed 3");
    shouldBe(re.exec("aaabc"), null, "Fails var min 2");
})();

// ==== Test 31: Global flag ====
(function() {
    var re = /(a){2,4}/g;
    var str = "aa aaa aaaa a aaaaa";
    var matches = str.match(re);
    shouldBe(matches.length, 4, "Should find 4 matches");
    shouldBe(matches[0], "aa", "First match");
    shouldBe(matches[1], "aaa", "Second match");
    shouldBe(matches[2], "aaaa", "Third match");
    shouldBe(matches[3], "aaaa", "Fourth match (from 5 a's)");
})();

// ==== Test 32: Replace ====
(function() {
    var re = /(a){2,4}/g;
    var result = "aa aaa aaaa a".replace(re, "X");
    shouldBe(result, "X X X a", "Replace should work correctly");
})();

// ==== Test 33: Infinite max with content backtracking ====
(function() {
    var re = /(a+){2,}b/;
    shouldBe(re.exec("aaab"), ["aaab", "a"], "3 a's: a+ splits for 2 iters + b");
    shouldBe(re.exec("aab"), ["aab", "a"], "2 a's: 2 iters of 1 a each + b");
    shouldBe(re.exec("ab"), null, "1 a: fails min 2");
    shouldBe(re.exec("aaaaab"), ["aaaaab", "a"], "5 a's + b");
})();

// ==== Test 34: Longer alternatives ====
(function() {
    var re = /(ab|abab){2,3}$/;
    shouldBe(re.exec("abababab"), ["abababab", "abab"], "8 chars: 2x'abab'");
    shouldBe(re.exec("abab"), ["abab", "ab"], "4 chars: 2x'ab'");
    shouldBe(re.exec("ab"), null, "2 chars: 1x'ab', fails min 2");
})();

// ==== Test 35: Non-capturing with content backtracking ====
(function() {
    var re = /(?:a+){2,4}b/;
    shouldBe(re.exec("aaab"), ["aaab"], "3 a's + b");
    shouldBe(re.exec("aab"), ["aab"], "2 a's + b");
    shouldBe(re.exec("ab"), null, "1 a: fails");
})();

// ==== Test 36: Nested quantifiers ====
(function() {
    var re = /((?:ab)+){2,3}c/;
    shouldBe(re.exec("ababababc"), ["ababababc", "ab"], "Nested: last iter captures 'ab'");
    shouldBe(re.exec("ababc"), ["ababc", "ab"], "Nested: 2 outer iters");
})();

// ==== Test 37: Backreferences with min > 0 ====
(function() {
    var re = /(a+)\1{2,3}/;
    shouldBe(re.exec("aaaaaa"), ["aaaaaa", "aa"], "Backref: 'aa' repeated 3 times");
})();

// ==== Test 38: Verify JIT and interpreter agree ====
// Run many times to trigger JIT compilation
(function() {
    for (var i = 0; i < 1000; i++) {
        var re = /(aa|aaaa){2,3}$/;
        var result = re.exec("aaaaaa");
        if (!result || result[0] !== "aaaaaa" || result[1] !== "aa")
            throw new Error("JIT/interpreter mismatch at iteration " + i + ": " + JSON.stringify(result ? Array.from(result) : null));
    }
})();

// ==== Test 39: Complex alternation requiring backtrack ====
(function() {
    var re = /(a|aa|aaa){2,3}$/;
    shouldBe(re.exec("aaaa"), ["aaaa", "aa"], "4 a's: 'aa'+'aa'");
    shouldBe(re.exec("aaa"), ["aaa", "a"], "3 a's: 'a'+'a'+'a' or 'a'+'aa'");
    shouldBe(re.exec("aa"), ["aa", "a"], "2 a's: 'a'+'a'");
    shouldBe(re.exec("a"), null, "1 a: fails min 2");
})();

// ==== Test 40: Greedy with inner content backtracking ====
(function() {
    var re = /(a+){2,4}aab/;
    shouldBe(re.exec("aaaab"), ["aaaab", "a"], "Need to save 'aab' for suffix");
    shouldBe(re.exec("aaaaab"), ["aaaaab", "a"], "More a's to distribute");
})();

// ==== Test 41: NonGreedy inner with min > 0 ====
(function() {
    var re = /(a+?){2,4}b/;
    shouldBe(re.exec("aaab"), ["aaab", "a"], "Non-greedy inner");
    shouldBe(re.exec("aab"), ["aab", "a"], "Non-greedy inner: 2 iters");
})();

// ==== Test 42: Zero-length inner match (min == 0, Greedy) ====
// (a?)* - inner can match empty string. min=0, so zero-length exit immediately.
(function() {
    shouldBe(/(a?)*b/.exec("b"), ["b", undefined], "(a?)*b on 'b': capture undefined");
    shouldBe(/(a?)*b/.exec("ab"), ["ab", "a"], "(a?)*b on 'ab': capture 'a'");
    shouldBe(/(a?)*b/.exec("aab"), ["aab", "a"], "(a?)*b on 'aab': capture 'a'");
})();

// ==== Test 43: Zero-length inner match (min > 0, Greedy) ====
// (a?)+ / (a?){1,2} - inner can match empty, forced iterations counted.
(function() {
    shouldBe(/(a?)+b/.exec("b"), ["b", ""], "(a?)+b on 'b': capture '' (forced iter)");
    shouldBe(/(a?)+b/.exec("ab"), ["ab", "a"], "(a?)+b on 'ab': capture 'a'");
    shouldBe(/(a?)+b/.exec("aab"), ["aab", "a"], "(a?)+b on 'aab': capture 'a'");

    shouldBe(/(a?){1,2}b/.exec("b"), ["b", ""], "(a?){1,2}b on 'b'");
    shouldBe(/(a?){1,2}b/.exec("ab"), ["ab", "a"], "(a?){1,2}b on 'ab'");
    shouldBe(/(a?){1,2}b/.exec("aab"), ["aab", "a"], "(a?){1,2}b on 'aab'");

    shouldBe(/(a?){2,3}b/.exec("b"), ["b", ""], "(a?){2,3}b on 'b'");
    shouldBe(/(a?){2,3}b/.exec("ab"), ["ab", ""], "(a?){2,3}b on 'ab'");
    shouldBe(/(a?){2,3}b/.exec("aab"), ["aab", "a"], "(a?){2,3}b on 'aab'");
    shouldBe(/(a?){2,3}b/.exec("aaab"), ["aaab", "a"], "(a?){2,3}b on 'aaab'");

    shouldBe(/(a?){1,}b/.exec("b"), ["b", ""], "(a?){1,}b on 'b'");
    shouldBe(/(a?){1,}b/.exec("ab"), ["ab", "a"], "(a?){1,}b on 'ab'");
})();

// ==== Test 44: Zero-length inner match (a*) ====
(function() {
    shouldBe(/(a*){2,3}b/.exec("b"), ["b", ""], "(a*){2,3}b on 'b'");
    shouldBe(/(a*){2,3}b/.exec("ab"), ["ab", ""], "(a*){2,3}b on 'ab'");
    shouldBe(/(a*){2,3}b/.exec("aab"), ["aab", ""], "(a*){2,3}b on 'aab'");
})();

// ==== Test 45: Zero-length inner match (NonGreedy) ====
(function() {
    shouldBe(/(a?){1,2}?b/.exec("b"), ["b", ""], "(a?){1,2}?b on 'b'");
    shouldBe(/(a?){1,2}?b/.exec("ab"), ["ab", "a"], "(a?){1,2}?b on 'ab'");
    shouldBe(/(a?){2,3}?b/.exec("b"), ["b", ""], "(a?){2,3}?b on 'b'");
    shouldBe(/(a?){2,3}?b/.exec("ab"), ["ab", ""], "(a?){2,3}?b on 'ab'");
    shouldBe(/(a?){2,3}?b/.exec("aab"), ["aab", "a"], "(a?){2,3}?b on 'aab'");
})();

// ==== Test 46: Zero-length inner match (min == 0, NonGreedy) ====
(function() {
    shouldBe(/(a?)*?b/.exec("b"), ["b", undefined], "(a?)*?b on 'b'");
    shouldBe(/(a?)*?b/.exec("ab"), ["ab", "a"], "(a?)*?b on 'ab'");
})();

// ==== Test 47: Zero-length with alternatives ====
(function() {
    shouldBe(/(a|){1,2}b/.exec("b"), ["b", ""], "(a|){1,2}b on 'b'");
    shouldBe(/(a|){1,2}b/.exec("ab"), ["ab", "a"], "(a|){1,2}b on 'ab'");
    shouldBe(/(a|){1,2}b/.exec("aab"), ["aab", "a"], "(a|){1,2}b on 'aab'");
})();

// ==== Test 48: Zero-length stress test with JIT ====
(function() {
    for (var i = 0; i < 1000; i++) {
        shouldBe(/(a?){1,2}b/.exec("ab"), ["ab", "a"], "JIT stress (a?){1,2}b on 'ab' iter " + i);
        shouldBe(/(a?)+b/.exec("b"), ["b", ""], "JIT stress (a?)+b on 'b' iter " + i);
        shouldBe(/(a?)*b/.exec("ab"), ["ab", "a"], "JIT stress (a?)*b on 'ab' iter " + i);
    }
})();

// ==== Test 49: Multiple capture groups with zero-length ====
(function() {
    shouldBe(/(a?)(b?){1,2}c/.exec("c"), ["c", "", ""], "(a?)(b?){1,2}c on 'c'");
    shouldBe(/(a?)(b?){1,2}c/.exec("abc"), ["abc", "a", "b"], "(a?)(b?){1,2}c on 'abc'");
    shouldBe(/(a?)(b?){1,2}c/.exec("ac"), ["ac", "a", ""], "(a?)(b?){1,2}c on 'ac'");
    shouldBe(/(a?)(b?){1,2}c/.exec("bc"), ["bc", "", "b"], "(a?)(b?){1,2}c on 'bc'");
})();
