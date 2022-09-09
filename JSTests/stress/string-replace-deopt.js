function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return string.replace("Hello", "World");
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test("Hello"), "World");

String.prototype[Symbol.replace] = function () { return "Changed"; };
for (var i = 0; i < 1e6; ++i)
    shouldBe(test("Hello"), "Changed");
