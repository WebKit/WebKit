// Regression test for bug where FixedCount parentheses could leave stale endIndex
// when a match failed mid-iteration. The issue was that clearSubpatternStart only
// cleared the start index, leaving end with garbage that could cause incorrect
// length calculations.

// Test 1: Negative lookahead with backreference to unset group in FixedCount
(function testNegativeLookaheadBackref() {
    var re = /(?!(\3){4,7})/gi;
    var s = "\u1DD2";
    var result = re.exec(s);
    // Should match at the beginning (negative lookahead succeeds because \3 is unset)
    if (result === null)
        throw new Error("Test 1 failed: expected match, got null");
    if (result.index !== 0)
        throw new Error("Test 1 failed: expected index 0, got " + result.index);
})();

// Test 2: Similar pattern with different character
(function testNegativeLookaheadBackref2() {
    var re = /(?!(\2){2,3})/;
    var result = re.test("x");
    if (result !== true)
        throw new Error("Test 2 failed: expected true, got " + result);
})();

// Test 3: FixedCount with nested groups that may partially match
(function testFixedCountNestedGroups() {
    var re = /(?:(a)|(b)){2}/;
    var result = "ab".match(re);
    if (!result)
        throw new Error("Test 3 failed: expected match, got null");
    if (result[0] !== "ab")
        throw new Error("Test 3 failed: expected 'ab', got " + result[0]);
    // result[1] should be undefined (last iteration matched 'b', not 'a')
    if (result[1] !== undefined)
        throw new Error("Test 3 failed: expected result[1] to be undefined, got " + result[1]);
    // result[2] should be 'b'
    if (result[2] !== "b")
        throw new Error("Test 3 failed: expected result[2] to be 'b', got " + result[2]);
})();

// Test 4: Run many iterations to trigger JIT
(function testManyIterations() {
    var re = /(?!(\3){4,7})/gi;
    for (var i = 0; i < 10000; i++) {
        var result = re.exec("\u1DD2");
        re.lastIndex = 0;
        if (result === null)
            throw new Error("Test 4 failed at iteration " + i);
    }
})();

// Test 5: Greedy quantifier with backreference
(function testGreedyWithBackref() {
    var re = /(?:(\w)(\1){3})+/;
    var result = "aaaa".match(re);
    if (!result)
        throw new Error("Test 5 failed: expected match, got null");
    if (result[0] !== "aaaa")
        throw new Error("Test 5 failed: expected 'aaaa', got " + result[0]);
})();

// Test 6: Empty backreference
(function testEmptyBackref() {
    var re = /(\1)*/;
    var result = re.exec("");
    if (result === null)
        throw new Error("Test 6 failed: expected match, got null");
})();
