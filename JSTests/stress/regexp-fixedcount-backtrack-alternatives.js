//@ runDefault

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Test cases that force backtracking between alternatives in FixedCount.
// The key is that the first alternative must fail AFTER matching, forcing
// a retry with the second alternative.

// Pattern: (?:aa|a){2}b
// Input: "aab"
// Naive matching: "aa" for first iteration, then we're at position 2, need another iteration
//   but only "b" left, can't match "aa" or "a" -> FAIL
// Correct matching with backtracking: "a" for first iteration (position 1),
//   "a" for second iteration (position 2), then "b" matches -> SUCCESS
(function() {
    var re = /(?:aa|a){2}b/;
    var result = re.exec("aab");
    shouldBe(result, ["aab"], "(?:aa|a){2}b on 'aab'");
})();

// Similar test with 3 alternatives
(function() {
    var re = /(?:aaa|aa|a){3}b/;
    var result = re.exec("aaab");
    shouldBe(result, ["aaab"], "(?:aaa|aa|a){3}b on 'aaab'");
})();

// Test where greedy first alternative consumes too much
(function() {
    var re = /(?:ab|a){2}c/;
    var result = re.exec("aac");
    shouldBe(result, ["aac"], "(?:ab|a){2}c on 'aac'");
})();

// Non-capturing FixedCount with 2 alternatives - simple case
(function() {
    var re = /(?:a|b){4}c/;
    var result = re.exec("ababc");
    shouldBe(result, ["ababc"], "(?:a|b){4}c on 'ababc'");
})();

// Non-capturing FixedCount with 3 alternatives
(function() {
    var re = /(?:a|b|c){3}d/;
    var result = re.exec("abcd");
    shouldBe(result, ["abcd"], "(?:a|b|c){3}d on 'abcd'");
})();

// Non-capturing FixedCount with alternatives, no match
(function() {
    var re = /(?:a|b){4}c/;
    var result = re.exec("aaac");
    shouldBe(result, null, "(?:a|b){4}c on 'aaac' (no match)");
})();
