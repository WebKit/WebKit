function testRegExp(pattern, string, expectedParseError, expectedMatch) {
    const r = new RegExp(pattern);
    let actualResult = null;
    let actualParseError = null;
    try {
        actualResult = r.exec(string);
    } catch(e) {
        actualParseError = e;
    }

    if (expectedParseError && expectedParseError != actualParseError)
        throw("Expected \"" + expectedParseError + "\", but got \"" + actualParseError + "\"");

    if (expectedMatch === undefined) {
        if (actualResult !== null)
            throw("Expected " + r + ".exec(\"" + string + "\") to be null");
    } else {
        if (actualResult === null || actualResult[0] !== expectedMatch)
            throw("Expected " + r + ".exec(\"" + string + "\")[0] to be " + expectedMatch + ".");
    }
}

testRegExp("a{0,4294967295}", "a", undefined, "a");
testRegExp("a{0,4294967296}", "a", undefined, "a");
testRegExp("^a{0,4294967296}$", "a{0,4294967296}", undefined, undefined);
testRegExp("(?:a{0,340282366920}?){0,1}a", "aa", undefined, "aa");
testRegExp("((.{100000000})*.{2100000000})+", "x", "SyntaxError: Invalid regular expression: pattern exceeds string length limits", undefined);
