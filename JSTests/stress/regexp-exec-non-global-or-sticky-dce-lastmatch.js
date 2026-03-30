function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

// RegExpExecNonGlobalOrSticky writes to RegExpState (RegExp.lastMatch, $1, etc.)
// but was missing NodeMustGenerate, allowing FTL DCE to eliminate it when
// the result is unused. This is the same bug pattern as
// https://bugs.webkit.org/show_bug.cgi?id=309953 (RegExpMatchFastGlobal).
//
// DFGStrengthReductionPhase converts RegExpExec to RegExpExecNonGlobalOrSticky
// via convertToRegExpExecNonGlobalOrStickyWithoutChecks when the regexp is a
// constant without the global or sticky flag.

function test(s) {
    /bc/.exec(s);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    /seed/.test("seeded");
    test("abcd");
    shouldBe(RegExp.lastMatch, "bc");
    shouldBe(RegExp.input, "abcd");
}

function testCapture(s) {
    /(b)(c)/.exec(s);
}
noInline(testCapture);

for (let i = 0; i < testLoopCount; ++i) {
    /(s)(e)/.test("seed");
    testCapture("abcd");
    shouldBe(RegExp.$1, "b");
    shouldBe(RegExp.$2, "c");
}

function testLeftRight(s) {
    /bc/.exec(s);
}
noInline(testLeftRight);

for (let i = 0; i < testLoopCount; ++i) {
    /seed/.test("seeded");
    testLeftRight("abcd");
    shouldBe(RegExp.leftContext, "a");
    shouldBe(RegExp.rightContext, "d");
}

function testNoMatch(s) {
    /xyz/.exec(s);
}
noInline(testNoMatch);

for (let i = 0; i < testLoopCount; ++i) {
    /seed/.test("seeded");
    testNoMatch("abcd");
    shouldBe(RegExp.lastMatch, "seed");
}
