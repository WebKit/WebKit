//@ requireOptions("--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("FAIL: got " + actual + ", expected " + expected);
}

function codePointAtIsUndefined(string, index) {
    return string.codePointAt(index) === undefined;
}
noInline(codePointAtIsUndefined);

function atIsUndefined(string, index) {
    return string.at(index) === undefined;
}
noInline(atIsUndefined);

function charCodeAtIsNaN(string, index) {
    var code = string.charCodeAt(index);
    return code !== code;
}
noInline(charCodeAtIsNaN);

function atNegative(string, index) {
    return string.at(index);
}
noInline(atNegative);

var str8Bit = "Hello, World!";
var str16Bit = "こんにちは世界";

for (var i = 0; i < testLoopCount; ++i) {
    var index = i % str8Bit.length;
    shouldBe(codePointAtIsUndefined(str8Bit, index), false);
    shouldBe(atIsUndefined(str8Bit, index), false);
    shouldBe(charCodeAtIsNaN(str8Bit, index), false);
    index = i % str16Bit.length;
    shouldBe(codePointAtIsUndefined(str16Bit, index), false);
    shouldBe(atIsUndefined(str16Bit, index), false);
    shouldBe(charCodeAtIsNaN(str16Bit, index), false);
}

for (var i = 0; i < 10; ++i) {
    shouldBe(codePointAtIsUndefined(str8Bit, str8Bit.length), true);
    shouldBe(atIsUndefined(str8Bit, str8Bit.length), true);
    shouldBe(charCodeAtIsNaN(str8Bit, str8Bit.length), true);
    shouldBe(codePointAtIsUndefined(str16Bit, str16Bit.length), true);
    shouldBe(atIsUndefined(str16Bit, str16Bit.length), true);
    shouldBe(charCodeAtIsNaN(str16Bit, str16Bit.length), true);
}

for (var i = 0; i < testLoopCount * 20; ++i) {
    var index = -(i % str8Bit.length) - 1;
    shouldBe(atNegative(str8Bit, index), str8Bit[str8Bit.length + index]);
    index = -(i % str16Bit.length) - 1;
    shouldBe(atNegative(str16Bit, index), str16Bit[str16Bit.length + index]);
}
shouldBe(atNegative(str8Bit, -str8Bit.length - 1), undefined);
shouldBe(atNegative(str16Bit, -str16Bit.length - 1), undefined);
