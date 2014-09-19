description("This test checks the ES6 string functions startsWith(), endsWith() and contains().");

// Test contains
shouldBe("'foo bar'.contains('bar')", "true");
shouldBe("'foo bar'.contains('bar', 4)", "true");
shouldBe("'foo bar'.contains('ar', 5)", "true");
shouldBe("'foo bar'.contains('qux')", "false");
shouldBe("'foo bar'.contains('foo')", "true");
shouldBe("'foo bar'.contains('foo', 0)", "true");
shouldBe("'foo bar'.contains('foo', -1)", "true");
shouldBe("'foo bar'.contains('')", "true");
shouldBe("'foo bar'.contains()", "false");
shouldBe("'foo bar qux'.contains('qux', 7)", "true");
shouldBe("'foo bar qux'.contains('bar', 7)", "false");
shouldBe("'foo null bar'.contains()", "false");
shouldBe("'foo null bar'.contains(null)", "true");
shouldBe("'foo null bar'.contains(null)", "true");
shouldBe("'foo undefined bar'.contains()", "true");
shouldBe("'foo undefined bar'.contains(undefined)", "true");
shouldBe("'foo undefined bar'.contains()", "true");
shouldBe("'foo undefined bar'.contains()", "true");
shouldBe("'foo true bar'.contains(true)", "true");
shouldBe("'foo false bar'.contains(false)", "true");
shouldBe("'foo 1 bar'.contains(1)", "true");
shouldBe("'foo 1.1 bar'.contains(1.1)", "true");
shouldBe("'foo NaN bar'.contains(NaN)", "true");
shouldBe("'foo 1.0 bar'.contains(1.0)", "true");
shouldBe("'foo 1e+100 bar'.contains(1e+100)", "true");
shouldBe("'foo 1e100 bar'.contains(1e100)", "false");
shouldBe("'フーバー'.contains('ーバ')", "true");
shouldBe("'フーバー'.contains('クー')", "false");

// Test startsWith
shouldBe("'foo bar'.startsWith('foo')", "true");
shouldBe("'foo bar'.startsWith('foo', 0)", "true");
shouldBe("'foo bar'.startsWith('foo', -1)", "true");
shouldBe("'foo bar'.startsWith('oo', 1)", "true");
shouldBe("'foo bar'.startsWith('qux')", "false");
shouldBe("'foo bar'.startsWith('')", "true");
shouldBe("'foo bar'.startsWith()", "false");
shouldBe("'null'.startsWith()", "false");
shouldBe("'null'.startsWith(null)", "true");
shouldBe("'null bar'.startsWith(null)", "true");
shouldBe("'undefined'.startsWith()", "true");
shouldBe("'undefined'.startsWith(undefined)", "true");
shouldBe("'undefined bar'.startsWith()", "true");
shouldBe("'undefined bar'.startsWith()", "true");
shouldBe("'true bar'.startsWith(true)", "true");
shouldBe("'false bar'.startsWith(false)", "true");
shouldBe("'1 bar'.startsWith(1)", "true");
shouldBe("'1.1 bar'.startsWith(1.1)", "true");
shouldBe("'NaN bar'.startsWith(NaN)", "true");
shouldBe("'1e+100 bar'.startsWith(1e+100)", "true");
shouldBe("'1e100 bar'.startsWith(1e100)", "false");
shouldBe("'フーバー'.startsWith('フー')", "true");
shouldBe("'フーバー'.startsWith('バー')", "false");

// Test endsWith
shouldBe("'foo bar'.endsWith('bar')", "true");
shouldBe("'foo bar'.endsWith('ba', 6)", "true");
shouldBe("'foo bar'.endsWith(' ba', 6)", "true");
shouldBe("'foo bar'.endsWith('foo bar')", "true");
shouldBe("'foo bar'.endsWith('foo bar', 7)", "true");
shouldBe("'foo bar'.endsWith('foo bar', 8)", "true");
shouldBe("'foo bar'.endsWith('foo bar', -1)", "false");
shouldBe("'foo bar'.endsWith('qux')", "false");
shouldBe("'foo bar'.endsWith('')", "true");
shouldBe("'foo bar'.endsWith()", "false");
shouldBe("'foo null'.endsWith()", "false");
shouldBe("'foo null'.endsWith(null)", "true");
shouldBe("'foo null'.endsWith(null)", "true");
shouldBe("'foo undefined'.endsWith()", "true");
shouldBe("'foo undefined'.endsWith(undefined)", "true");
shouldBe("'foo undefined'.endsWith()", "true");
shouldBe("'foo undefined'.endsWith()", "true");
shouldBe("'foo true'.endsWith(true)", "true");
shouldBe("'foo false'.endsWith(false)", "true");
shouldBe("'foo 1'.endsWith(1)", "true");
shouldBe("'foo 1.1'.endsWith(1.1)", "true");
shouldBe("'foo NaN'.endsWith(NaN)", "true");
shouldBe("'foo 1e+100'.endsWith(1e+100)", "true");
shouldBe("'foo 1e100'.endsWith(1e100)", "false");
shouldBe("'フーバー'.endsWith('バー')", "true");
shouldBe("'フーバー'.endsWith('フー')", "false");

// Call functions with an environment record as 'this'.
shouldThrow("(function() { var f = String.prototype.startsWith; (function() { f('a'); })(); })()");
shouldThrow("(function() { var f = String.prototype.endsWith; (function() { f('a'); })(); })()");
shouldThrow("(function() { var f = String.prototype.contains; (function() { f('a'); })(); })()");

// ES6 spec says a regex as argument should throw an Exception.
shouldThrow("'foo bar'.startsWith(/\w+/)");
shouldThrow("'foo bar'.endsWith(/\w+/)");
shouldThrow("'foo bar'.contains(/\w+/)");

// Check side effects in startsWith.
var sideEffect = "";
var stringToSearchIn = new String("foo bar");
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
var startOffset = new Number(0);
startOffset.valueOf = function() {
    sideEffect += "B";
    return this;
}
var matchString = new String("foo");
matchString.toString = function() {
    sideEffect += "C";
    return this;
}
// Calling stringToSearchIn.startsWith implicitly calls stringToSearchIn.toString(),
// startOffset.valueOf() and matchString.toString(), in that respective order.
shouldBe("stringToSearchIn.startsWith(matchString, startOffset)", "true");
shouldBe("sideEffect == 'ABC'", "true");

// If stringToSearchIn throws an exception startOffset.valueOf() and
// matchString.toString() are not called.
stringToSearchIn.toString = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.startsWith(matchString, startOffset)", "'error'");
shouldBe("sideEffect == ''", "true");

// If startOffset throws an exception stringToSearchIn.toString() is called but
// matchString.toString() is not.
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
startOffset.valueOf = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.startsWith(matchString, startOffset)", "'error'");
shouldBe("sideEffect == 'A'", "true");

// If matchString.toString() throws an exception stringToSearchIn.toString() and
// startOffset.valueOf() were called.
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
startOffset.valueOf = function() {
    sideEffect += "B";
    return this;
}
matchString.toString = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.startsWith(matchString, startOffset)", "'error'");
shouldBe("sideEffect == 'AB'", "true");

// Check side effects in endsWith.
sideEffect = "";
stringToSearchIn = new String('foo bar');
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
var endOffset = new Number(stringToSearchIn.length);
endOffset.valueOf = function() {
    sideEffect += "B";
    return this;
}
matchString = new String('bar');
matchString.toString = function() {
    sideEffect += "C";
    return this;
}

// Calling stringToSearchIn.endsWith implicitly calls stringToSearchIn.toString(),
// endOffset.valueOf() and matchString.toString(), in that respective order.
shouldBe("stringToSearchIn.endsWith(matchString, endOffset)", "true");
shouldBe("sideEffect == 'ABC'", "true");

// If stringToSearchIn throws an exception endOffset.valueOf() and
// matchString.toString() are not called.
stringToSearchIn.toString = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.endsWith(matchString, endOffset)", "'error'");
shouldBe("sideEffect == ''", "true");

// If endOffset throws an exception stringToSearchIn.toString() is called but
// matchString.toString() is not.
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
endOffset.valueOf = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.endsWith(matchString, endOffset)", "'error'");
shouldBe("sideEffect == 'A'", "true");

// If matchString.toString() throws an exception stringToSearchIn.toString() and
// endOffset.valueOf() were called.
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
endOffset.valueOf = function() {
    sideEffect += "B";
    return this;
}
matchString.toString = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.endsWith(matchString, endOffset)", "'error'");
shouldBe("sideEffect == 'AB'", "true");

// Check side effects in contains.
var sideEffect = "";
stringToSearchIn = new String("foo bar");
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
var startOffset = new Number(0);
startOffset.valueOf = function() {
    sideEffect += "B";
    return this;
}
matchString = new String("foo");
matchString.toString = function() {
    sideEffect += "C";
    return this;
}
// Calling stringToSearchIn.contains implicitly calls stringToSearchIn.toString(),
// startOffset.valueOf() and matchString.toString(), in that respective order.
shouldBe("stringToSearchIn.contains(matchString, startOffset)", "true");
shouldBe("sideEffect == 'ABC'", "true");

// If stringToSearchIn throws an exception startOffset.valueOf() and
// matchString.toString() are not called.
stringToSearchIn.toString = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.contains(matchString, startOffset)", "'error'");
shouldBe("sideEffect == ''", "true");

// If startOffset throws an exception stringToSearchIn.toString() is called but
// matchString.toString() is not.
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
startOffset.valueOf = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.contains(matchString, startOffset)", "'error'");
shouldBe("sideEffect == 'A'", "true");

// If matchString.toString() throws an exception stringToSearchIn.toString() and
// startOffset.valueOf() were called.
stringToSearchIn.toString = function() {
    sideEffect += "A";
    return this;
}
startOffset.valueOf = function() {
    sideEffect += "B";
    return this;
}
matchString.toString = function() {
    throw "error";
}
sideEffect = "";
shouldThrow("stringToSearchIn.contains(matchString, startOffset)", "'error'");
shouldBe("sideEffect == 'AB'", "true");
