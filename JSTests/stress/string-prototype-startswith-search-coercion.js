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
    return string.startsWith(search);
}
noInline(test);

function testWithIndex(string, search, index) {
    return string.startsWith(search, index);
}
noInline(testWithIndex);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("HelloWorld123true[object Object]null");

for (var i = 0; i < testLoopCount; ++i) {
    // Number coercion to string
    shouldBe(test(makeString("123Hello"), 123), true);
    shouldBe(test(makeString("123Hello"), 124), false);
    shouldBe(test(string, 123), false);
    shouldBe(testWithIndex(string, 123, 10), true);

    // Boolean coercion to string
    shouldBe(test(makeString("trueHello"), true), true);
    shouldBe(test(makeString("falseHello"), false), true);
    shouldBe(test(makeString("trueHello"), false), false);
    shouldBe(testWithIndex(string, true, 13), true);

    // null coercion to string
    shouldBe(test(makeString("nullHello"), null), true);
    shouldBe(test(string, null), false);
    shouldBe(testWithIndex(string, null, 32), true);

    // undefined coercion to string
    shouldBe(test(makeString("undefinedHello"), undefined), true);
    shouldBe(test(string, undefined), false);

    // Object coercion via toString
    var objWithToString = { toString: function() { return "Hello"; } };
    shouldBe(test(string, objWithToString), true);
    shouldBe(test(makeString("World"), objWithToString), false);

    // Object coercion via valueOf (toString takes precedence for string conversion)
    var objWithValueOf = { valueOf: function() { return "Hello"; }, toString: function() { return "World"; } };
    shouldBe(test(string, objWithValueOf), false);
    shouldBe(test(makeString("WorldHello"), objWithValueOf), true);

    // Plain object "[object Object]"
    var plainObj = {};
    shouldBe(test(string, plainObj), false);
    shouldBe(testWithIndex(string, plainObj, 17), true);

    // Array coercion
    shouldBe(test(makeString("Hello"), ["Hello"]), true);
    shouldBe(test(makeString("1,2,3Hello"), [1, 2, 3]), true);
    shouldBe(test(makeString("Hello"), [1, 2, 3]), false);

    // Empty array
    shouldBe(test(makeString("Hello"), []), true);

    // Symbol should throw TypeError
    shouldThrow(function() {
        test(string, Symbol("test"));
    }, TypeError);

    shouldThrow(function() {
        testWithIndex(string, Symbol.iterator, 0);
    }, TypeError);

    // RegExp should throw TypeError (per spec)
    shouldThrow(function() {
        test(string, /Hello/);
    }, TypeError);

    // RegExp with Symbol.match set to falsy should work
    var regexLike = /Hello/;
    regexLike[Symbol.match] = false;
    shouldBe(test(makeString("/Hello/Hello"), regexLike), true);
}
