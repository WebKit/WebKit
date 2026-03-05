//@ runDefault

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Test 1: Basic fixed count with capture
(function() {
    var re = /^From\s+\S+\s+([a-zA-Z]{3}\s+){2}\d{1,2}\s+\d\d:\d\d/;
    var result = re.exec("From abcd  Mon Sep 01 12:33:02 1997");
    shouldBe(result[0], "From abcd  Mon Sep 01 12:33", "Test 1 full match");
    shouldBe(result[1], "Sep ", "Test 1 capture");
})();

// Test 2: Backreference requiring within-iteration backtracking (main failing case)
(function() {
    var re = /(([a-c])b*?\2){3}/;
    var result = re.exec("ababbbcbc");
    shouldBe(result, ["ababbbcbc", "cbc", "c"], "Test 2 backreference");
})();

// Test 3: Simple fixed count capture
(function() {
    var re = /((x)){2}y/;
    var result = re.exec("xxy");
    shouldBe(result, ["xxy", "x", "x"], "Test 3 simple capture");
})();

// Test 4: Fixed count with no match
(function() {
    var re = /((x)){2}y/;
    var result = re.exec("xxz");
    shouldBe(result, null, "Test 4 no match");
})();

// Test 5: Alternatives inside fixed group
(function() {
    var re = /(x|y){3}z/;
    var result = re.exec("xxyz");
    shouldBe(result, ["xxyz", "y"], "Test 5 alternatives");
})();

// Test 6: Greedy inside fixed count (reference behavior from V8/interpreter)
(function() {
    var re = /(a+){2}b/;
    var result = re.exec("aaab");
    shouldBe(result, ["aaab", "a"], "Test 6 greedy inside fixed");
})();

// Test 7: Non-greedy inside fixed count
(function() {
    var re = /(a+?){2}b/;
    var result = re.exec("aaab");
    shouldBe(result, ["aaab", "aa"], "Test 7 non-greedy inside fixed");
})();

// Test 8: Deeply nested captures
(function() {
    var re = /(((a)b)+){2}c/;
    var result = re.exec("ababababc");
    // Iter1 of {2} matches "ababab" (greedy + inside)
    // Iter2 of {2} matches "ab"
    // Last capture of group 1 is "ab", group 2 is "ab", group 3 is "a"
    shouldBe(result, ["ababababc", "ab", "ab", "a"], "Test 8 nested");
})();

// Test 9: Non-capturing fixed count (uses simpler FixedCount opcodes)
(function() {
    var re = /(?:abc){3}d/;
    var result = re.exec("abcabcabcd");
    shouldBe(result, ["abcabcabcd"], "Test 9 non-capturing fixed count");
})();

// Test 10: Non-capturing fixed count with alternatives
(function() {
    var re = /(?:a|b){4}c/;
    var result = re.exec("ababc");
    shouldBe(result, ["ababc"], "Test 10 non-capturing with alternatives");
})();

// Test 11: Non-capturing fixed count no match
(function() {
    var re = /(?:abc){3}d/;
    var result = re.exec("abcabcd");
    shouldBe(result, null, "Test 11 non-capturing no match");
})();

// Test 12: Mixed capturing and non-capturing
(function() {
    var re = /(?:x){2}(y){2}z/;
    var result = re.exec("xxyyz");
    shouldBe(result, ["xxyyz", "y"], "Test 12 mixed capturing and non-capturing");
})();
