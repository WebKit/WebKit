function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search, endPosition) {
    return string.endsWith(search, endPosition);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("HelloWorld");
var search = makeString("Hello");

for (var i = 0; i < testLoopCount; ++i) {
    // NaN should be treated as 0 (NaN converts to 0 in ToIntegerOrInfinity)
    shouldBe(test(string, makeString(""), NaN), true);
    shouldBe(test(string, search, NaN), false);
    shouldBe(test(string, makeString("H"), NaN), false);

    // Infinity should be clamped to string length
    shouldBe(test(string, makeString("World"), Infinity), true);
    shouldBe(test(string, search, Infinity), false);
    shouldBe(test(string, makeString("HelloWorld"), Infinity), true);

    // -Infinity should be treated as 0
    shouldBe(test(string, makeString(""), -Infinity), true);
    shouldBe(test(string, search, -Infinity), false);

    // Floating point numbers should be truncated
    shouldBe(test(string, search, 5.0), true);
    shouldBe(test(string, search, 5.9), true);
    shouldBe(test(string, search, 5.1), true);
    shouldBe(test(string, search, 4.9), false);
    shouldBe(test(string, makeString("ello"), 5.0), true);
    shouldBe(test(string, makeString("ello"), 4.0), false);

    // Negative floating point
    shouldBe(test(string, makeString(""), -0.5), true);
    shouldBe(test(string, search, -0.5), false);

    // undefined should be treated as string.length
    shouldBe(test(string, makeString("World"), undefined), true);
    shouldBe(test(string, search, undefined), false);

    // null should be treated as 0
    shouldBe(test(string, makeString(""), null), true);
    shouldBe(test(string, search, null), false);

    // Boolean coercion
    shouldBe(test(string, makeString(""), false), true);
    shouldBe(test(string, makeString("H"), true), true);
    shouldBe(test(string, search, true), false);

    // String coercion
    shouldBe(test(string, makeString(""), "0"), true);
    shouldBe(test(string, search, "5"), true);
    shouldBe(test(string, search, "5.9"), true);
    shouldBe(test(string, makeString("World"), "notanumber"), false); // "notanumber" -> NaN -> 0

    // Object with valueOf
    shouldBe(test(string, search, { valueOf: function() { return 5; } }), true);
    shouldBe(test(string, search, { valueOf: function() { return 4; } }), false);

    // Object with toString
    shouldBe(test(string, search, { toString: function() { return "5"; } }), true);
}
