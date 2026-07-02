function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function testInBounds(string) {
    var sum = 0;
    for (var i = 0; i < string.length; ++i) {
        var c = string.at(i);
        sum += c.charCodeAt(0);
    }
    return sum;
}
noInline(testInBounds);

function testNegative(string) {
    var sum = 0;
    for (var i = 1; i <= string.length; ++i) {
        var c = string.at(-i);
        sum += c.charCodeAt(0);
    }
    return sum;
}
noInline(testNegative);

function testOutOfBounds(string) {
    var sum = 0;
    for (var i = 0; i < string.length * 2; ++i) {
        var c = string.at(i);
        if (c !== undefined)
            sum += c.charCodeAt(0);
    }
    return sum;
}
noInline(testOutOfBounds);

var str8bit = "Hello, World!";
var str16bit = "こんにちは世界";

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testInBounds(str8bit), 1129);
    shouldBe(testNegative(str8bit), 1129);
    shouldBe(testOutOfBounds(str8bit), 1129);
    shouldBe(testInBounds(str16bit), 112003);
    shouldBe(testNegative(str16bit), 112003);
    shouldBe(testOutOfBounds(str16bit), 112003);
}
