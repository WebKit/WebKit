function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string)
{
    return new String(string);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var object = test("Cocoa");
    shouldBe(object instanceof String, true);
    shouldBe(object.valueOf(), "Cocoa");
}
