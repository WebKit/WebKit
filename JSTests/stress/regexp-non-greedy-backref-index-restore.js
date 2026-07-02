function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL: " + msg + " expected: " + JSON.stringify(expected) + " actual: " + JSON.stringify(actual));
}

function testRegExp(re, str, expectedMatch, expectedIndex) {
    for (var i = 0; i < 200; i++) {
        var result = re.exec(str);
        if (expectedMatch === null) {
            shouldBe(result, null, re + ".exec(" + JSON.stringify(str) + ")");
        } else {
            shouldBe(result !== null, true, re + ".exec(" + JSON.stringify(str) + ") should not be null");
            for (var j = 0; j < expectedMatch.length; j++)
                shouldBe(result[j], expectedMatch[j], re + ".exec(" + JSON.stringify(str) + ")[" + j + "]");
            shouldBe(result.index, expectedIndex, re + ".exec(" + JSON.stringify(str) + ").index");
        }
    }
}

testRegExp(/(a)\1{0,2}?$/, "aaaa", ["aaa", "a"], 1);
testRegExp(/(a)\1{0,2}?$/, "aaaaa", ["aaa", "a"], 2);
testRegExp(/(a)\1??$/, "aaa", ["aa", "a"], 1);
testRegExp(/(ab)\1{0,2}?$/, "abababab", ["ababab", "ab"], 2);
testRegExp(/(.)\1{0,2}?$/, "xaaaa", ["aaa", "a"], 2);
testRegExp(/(a)\1{0,1}?$/, "aaaaa", ["aa", "a"], 3);
testRegExp(/(a)\1{0,3}?$/, "aaaaa", ["aaaa", "a"], 1);
testRegExp(/(a)\1{0,2}?/, "aaaa", ["a", "a"], 0);
testRegExp(/(a)\1{0,2}$/, "aaaa", ["aaa", "a"], 1);

// Alternation: NonGreedy backreference fails, falls through to next alternative
testRegExp(/(a)\1*?b|c/, "aac", ["c", undefined], 2);
testRegExp(/(a)\1*?b|aac/, "aac", ["aac", undefined], 0);

// Multiple backreferences
testRegExp(/(a)(b)\2*?\1/, "abba", ["abba", "a", "b"], 0);

// Undefined capture with NonGreedy
testRegExp(/(a)?\1*?b/, "b", ["b", undefined], 0);
testRegExp(/(a)?\1*?b/, "aab", ["aab", "a"], 0);

// Nested group with NonGreedy backreference and anchor
testRegExp(/(a)(b)\2*?\1$/, "abbbba", ["abbbba", "a", "b"], 0);
