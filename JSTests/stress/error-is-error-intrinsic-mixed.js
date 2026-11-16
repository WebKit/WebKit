function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function test(v) {
    return Error.isError(v);
}
noInline(test);

class MyError extends Error {}

var err = new Error();
var typeErr = new TypeError();
var myErr = new MyError();
var obj = {};
var arr = [];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(err), true);
    shouldBe(test(typeErr), true);
    shouldBe(test(myErr), true);
    shouldBe(test(obj), false);
    shouldBe(test(arr), false);
    shouldBe(test(undefined), false);
    shouldBe(test(null), false);
    shouldBe(test(42), false);
    shouldBe(test("str"), false);
}

function testCellOnly(v) {
    return Error.isError(v);
}
noInline(testCellOnly);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testCellOnly(err), true);
    shouldBe(testCellOnly(obj), false);
}

function testNonCellOnly(v) {
    return Error.isError(v);
}
noInline(testNonCellOnly);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(testNonCellOnly(i), false);
