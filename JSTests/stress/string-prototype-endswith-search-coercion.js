function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrow(func, expectedError) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('Expected error: ' + expectedError);
    if (!(error instanceof expectedError))
        throw new Error('Wrong error type: ' + error.constructor.name + ' expected: ' + expectedError.name);
}

function test(string, search) {
    return string.endsWith(search);
}
noInline(test);

function testWithEndPosition(string, search, endPosition) {
    return string.endsWith(search, endPosition);
}
noInline(testWithEndPosition);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("null[object Object]true123HelloWorld");

for (var i = 0; i < testLoopCount; ++i) {
    // Number coercion to string
    shouldBe(test(makeString("Hello123"), 123), true);
    shouldBe(test(makeString("Hello124"), 124), true);
    shouldBe(test(string, 123), false);
    shouldBe(testWithEndPosition(string, 123, 26), true);

    // Boolean coercion to string
    shouldBe(test(makeString("Hellotrue"), true), true);
    shouldBe(test(makeString("Hellofalse"), false), true);
    shouldBe(test(makeString("Hellotrue"), false), false);
    shouldBe(testWithEndPosition(string, true, 23), true);

    // null coercion to string
    shouldBe(test(makeString("Hellonull"), null), true);
    shouldBe(test(string, null), false);
    shouldBe(testWithEndPosition(string, null, 4), true);

    // undefined coercion to string
    shouldBe(test(makeString("Helloundefined"), undefined), true);
    shouldBe(test(string, undefined), false);

    // Object coercion via toString
    var objWithToString = { toString: function() { return "World"; } };
    shouldBe(test(string, objWithToString), true);
    shouldBe(test(makeString("Hello"), objWithToString), false);

    // Object coercion via valueOf (toString takes precedence for string conversion)
    var objWithValueOf = { valueOf: function() { return "World"; }, toString: function() { return "Hello"; } };
    shouldBe(test(string, objWithValueOf), false);
    shouldBe(test(makeString("WorldHello"), objWithValueOf), true);

    // Plain object "[object Object]"
    var plainObj = {};
    shouldBe(test(string, plainObj), false);
    shouldBe(testWithEndPosition(string, plainObj, 19), true);

    // Array coercion
    shouldBe(test(makeString("Hello"), ["Hello"]), true);
    shouldBe(test(makeString("Hello1,2,3"), [1, 2, 3]), true);
    shouldBe(test(makeString("Hello"), [1, 2, 3]), false);

    // Empty array
    shouldBe(test(makeString("Hello"), []), true);

    // Symbol should throw TypeError
    shouldThrow(function() {
        test(string, Symbol("test"));
    }, TypeError);

    shouldThrow(function() {
        testWithEndPosition(string, Symbol.iterator, 0);
    }, TypeError);

    // RegExp should throw TypeError (per spec)
    shouldThrow(function() {
        test(string, /World/);
    }, TypeError);

    // RegExp with Symbol.match set to falsy should work
    var regexLike = /World/;
    regexLike[Symbol.match] = false;
    shouldBe(test(makeString("Hello/World/"), regexLike), true);
}
