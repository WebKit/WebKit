// Tests that constant string length is folded in DFG/FTL.
// No output on success, throws on failure.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected " + expected + " but got " + actual);
}

// .length
function testLength() {
    return "hello".length;
}

// .charCodeAt
function testCharCodeAt() {
    return "hello".charCodeAt(1);
}

// .codePointAt
function testCodePointAt() {
    return "hello".codePointAt(0);
}

// .slice
function testSlice() {
    return "hello world".slice(6);
}

// .at (negative index uses length internally)
function testAt() {
    return "hello".at(-1);
}

// .toUpperCase
function testToUpperCase() {
    return "HELLO".toUpperCase();
}

// .toLowerCase
function testToLowerCase() {
    return "hello".toLowerCase();
}

// Long string > 10000 chars to exercise the dynamicCastConstant fallback
// (tryGetString returns null for strings > 10000 chars)
var longStr = "";
for (var i = 0; i < 10001; i++)
    longStr += "a";
// Use eval to make the string a compile-time constant in the function
var testLongLength = new Function("return " + JSON.stringify(longStr) + ".length;");

// Run enough iterations to trigger FTL
for (var i = 0; i < 1e5; i++) {
    shouldBe(testLength(), 5);
    shouldBe(testCharCodeAt(), 101); // 'e'
    shouldBe(testCodePointAt(), 104); // 'h'
    shouldBe(testSlice(), "world");
    shouldBe(testAt(), "o");
    shouldBe(testToUpperCase(), "HELLO");
    shouldBe(testToLowerCase(), "hello");
    shouldBe(testLongLength(), 10001);
}
