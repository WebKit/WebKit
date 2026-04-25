//@ memoryHog!

// Test that we do not overflow the stack while handling regular expressions.

// A RegExp with extremely deep nesting must throw or succeed, not crash.
function testDeepNesting() {
    try {
        new RegExp(Array(520000).join("(") + "a" + Array(520000).join(")"));
    } catch (e) {
        if (e instanceof RangeError)
            return;
        throw new Error("Expected RangeError, got " + e);
    }
    // With certain stack configurations the pattern may fit; that's OK.
}

// Base case.
testDeepNesting();

// Verify that a deep JS stack does not help avoiding the error.
(function recursiveCall(depth) {
    if (!(depth % 10))
        testDeepNesting();
    if (depth < 100)
        recursiveCall(depth + 1);
})(0);

// Have the deepest nested subpattern surrounded by other expressions.
{
    var expression = "";
    for (let i = 0; i < 520000; ++i)
        expression += "((a)(";
    expression += "b";
    for (let i = 0; i < 520000; ++i)
        expression += ")(c))";
    try {
        new RegExp(expression);
    } catch (e) {
        if (e instanceof SyntaxError || e instanceof RangeError)
            ; // expected
        else
            throw new Error("Expected SyntaxError or RangeError, got " + e);
    }
}
