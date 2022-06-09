function testRegExp(pattern, string, expectedMatch) {
    const r = new RegExp(pattern);
    const actualResult = r.exec(string);
    if (expectedMatch === undefined) {
        if (actualResult !== null)
            throw("Expected " + r + ".exec(\"" + string + "\") to be null");
    } else {
        if (actualResult === null || actualResult[0] !== expectedMatch)
            throw("Expected " + r + ".exec(\"" + string + "\")[0] to be " + expectedMatch + ".");
    }
}

testRegExp("a{0,4294967295}", "a", "a");
testRegExp("a{0,4294967296}", "a", "a");
testRegExp("^a{0,4294967296}$", "a{0,4294967296}", undefined);
testRegExp("(?:a{0,340282366920}?){0,1}a", "aa", "aa");
