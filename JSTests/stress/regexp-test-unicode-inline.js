// Test that RegExp.test() with unicode flag can be inlined in DFG/FTL.
// InlineTest always compiles for Char8 (Latin1) strings only, so surrogate pair
// decoding is not needed and the /u flag should not prevent inlining.

function testBasic(s) {
    return /abc/u.test(s);
}
noInline(testBasic);

function testIgnoreCase(s) {
    return /abc/iu.test(s);
}
noInline(testIgnoreCase);

function testWordClass(s) {
    return /\w+/u.test(s);
}
noInline(testWordClass);

function testDigitClass(s) {
    return /\d+/u.test(s);
}
noInline(testDigitClass);

function testPropertyEscape(s) {
    return /\p{Letter}/u.test(s);
}
noInline(testPropertyEscape);

function testDot(s) {
    return /a.c/u.test(s);
}
noInline(testDot);

function testWordBoundary(s) {
    return /\bfoo\b/iu.test(s);
}
noInline(testWordBoundary);

function testNonBMP(s) {
    return /\u{1F600}/u.test(s);
}
noInline(testNonBMP);

function testUnicodeSets(s) {
    return /[a-z]/v.test(s);
}
noInline(testUnicodeSets);

function testCharClassWithProperty(s) {
    return /[a-z\p{Number}]/u.test(s);
}
noInline(testCharClassWithProperty);

for (var i = 0; i < testLoopCount; ++i) {
    // Basic unicode pattern
    if (!testBasic("abc"))
        throw "Error: testBasic should match 'abc'";
    if (!testBasic("xabcy"))
        throw "Error: testBasic should match 'xabcy'";
    if (testBasic("abd"))
        throw "Error: testBasic should not match 'abd'";
    if (testBasic(""))
        throw "Error: testBasic should not match empty string";

    // Unicode + ignoreCase
    if (!testIgnoreCase("ABC"))
        throw "Error: testIgnoreCase should match 'ABC'";
    if (!testIgnoreCase("AbC"))
        throw "Error: testIgnoreCase should match 'AbC'";
    if (testIgnoreCase("abd"))
        throw "Error: testIgnoreCase should not match 'abd'";

    // Unicode word class
    if (!testWordClass("hello"))
        throw "Error: testWordClass should match 'hello'";
    if (!testWordClass("a"))
        throw "Error: testWordClass should match 'a'";
    if (testWordClass("   "))
        throw "Error: testWordClass should not match spaces";

    // Unicode digit class
    if (!testDigitClass("123"))
        throw "Error: testDigitClass should match '123'";
    if (testDigitClass("abc"))
        throw "Error: testDigitClass should not match 'abc'";

    // Unicode property escape
    if (!testPropertyEscape("a"))
        throw "Error: testPropertyEscape should match 'a'";
    if (!testPropertyEscape("Z"))
        throw "Error: testPropertyEscape should match 'Z'";
    if (testPropertyEscape("1"))
        throw "Error: testPropertyEscape should not match '1'";

    // Dot in unicode mode
    if (!testDot("abc"))
        throw "Error: testDot should match 'abc'";
    if (!testDot("axc"))
        throw "Error: testDot should match 'axc'";
    if (testDot("ac"))
        throw "Error: testDot should not match 'ac'";

    // Word boundary with unicode + ignoreCase
    if (!testWordBoundary("foo"))
        throw "Error: testWordBoundary should match 'foo'";
    if (!testWordBoundary("FOO"))
        throw "Error: testWordBoundary should match 'FOO'";
    if (!testWordBoundary("x foo y"))
        throw "Error: testWordBoundary should match 'x foo y'";
    if (testWordBoundary("foobar"))
        throw "Error: testWordBoundary should not match 'foobar'";

    // Non-BMP pattern against 8-bit string should not match
    if (testNonBMP("abc"))
        throw "Error: testNonBMP should not match 'abc'";
    if (testNonBMP(""))
        throw "Error: testNonBMP should not match empty string";

    // unicodeSets flag
    if (!testUnicodeSets("a"))
        throw "Error: testUnicodeSets should match 'a'";
    if (!testUnicodeSets("z"))
        throw "Error: testUnicodeSets should match 'z'";
    if (testUnicodeSets("A"))
        throw "Error: testUnicodeSets should not match 'A'";
    if (testUnicodeSets("1"))
        throw "Error: testUnicodeSets should not match '1'";

    // Character class with unicode property
    if (!testCharClassWithProperty("a"))
        throw "Error: testCharClassWithProperty should match 'a'";
    if (!testCharClassWithProperty("5"))
        throw "Error: testCharClassWithProperty should match '5'";
    if (testCharClassWithProperty("!"))
        throw "Error: testCharClassWithProperty should not match '!'";
}
