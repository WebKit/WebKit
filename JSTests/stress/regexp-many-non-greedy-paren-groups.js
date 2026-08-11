// Test that regular expressions with many sequential non-greedy quantified
// parenthesized groups produce correct results at various sizes.

function testLargeNonGreedyParens(n) {
    let s = '(?:a){0,2}?'.repeat(n);

    let r = new RegExp(s);

    let result = 'aaa'.match(r);
    if (result === null)
        throw new Error("Expected match for n=" + n);
    if (result.index !== 0)
        throw new Error("Expected index 0 for n=" + n + ", got " + result.index);

    let replaced = 'a'.replace(r, 'x');
    if (typeof replaced !== 'string')
        throw new Error("replace failed for n=" + n);
}

testLargeNonGreedyParens(10);
testLargeNonGreedyParens(100);
testLargeNonGreedyParens(1000);
testLargeNonGreedyParens(2000);
testLargeNonGreedyParens(4000);
testLargeNonGreedyParens(8193);
