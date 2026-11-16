function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string, pattern, replacement) {
    return string.replace(pattern, replacement);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test("Hello", "Hello", "World"), "World");

Object.prototype[Symbol.replace] = function () { return "Changed"; };
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test("Hello", "Hello", "World"), "World");

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test("Hello", {}, "World"), "Changed");
