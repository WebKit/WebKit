function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search, index) {
    return string.startsWith(search, index);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("HelloWorld");
var search = makeString("World");

for (var i = 0; i < testLoopCount; ++i) {
    // NaN should be treated as 0
    shouldBe(test(string, makeString("Hello"), NaN), true);
    shouldBe(test(string, search, NaN), false);

    // Infinity should be clamped to string length
    shouldBe(test(string, makeString(""), Infinity), true);
    shouldBe(test(string, search, Infinity), false);
    shouldBe(test(string, makeString("Hello"), Infinity), false);

    // -Infinity should be treated as 0
    shouldBe(test(string, makeString("Hello"), -Infinity), true);
    shouldBe(test(string, search, -Infinity), false);

    // Floating point numbers should be truncated
    shouldBe(test(string, search, 5.0), true);
    shouldBe(test(string, search, 5.9), true);
    shouldBe(test(string, search, 5.1), true);
    shouldBe(test(string, makeString("Hello"), 0.9), true);
    shouldBe(test(string, makeString("ello"), 0.9), false);
    shouldBe(test(string, makeString("ello"), 1.0), true);

    // Negative floating point
    shouldBe(test(string, makeString("Hello"), -0.5), true);
    shouldBe(test(string, makeString("Hello"), -1.5), true);

    // undefined should be treated as 0
    shouldBe(test(string, makeString("Hello"), undefined), true);
    shouldBe(test(string, search, undefined), false);

    // null should be treated as 0
    shouldBe(test(string, makeString("Hello"), null), true);
    shouldBe(test(string, search, null), false);

    // Boolean coercion
    shouldBe(test(string, makeString("Hello"), false), true);
    shouldBe(test(string, makeString("ello"), true), true);
    shouldBe(test(string, makeString("Hello"), true), false);

    // String coercion
    shouldBe(test(string, makeString("Hello"), "0"), true);
    shouldBe(test(string, search, "5"), true);
    shouldBe(test(string, search, "5.9"), true);
    shouldBe(test(string, makeString("Hello"), "notanumber"), true); // "notanumber" -> NaN -> 0

    // Object with valueOf
    shouldBe(test(string, search, { valueOf: function() { return 5; } }), true);
    shouldBe(test(string, search, { valueOf: function() { return 4; } }), false);

    // Object with toString
    shouldBe(test(string, search, { toString: function() { return "5"; } }), true);
}
