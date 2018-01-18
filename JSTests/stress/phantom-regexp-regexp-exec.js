function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string, flag)
{
    var regexp = /oa/;
    var result = regexp.exec(string);
    if (flag)
        return regexp;
    return result;
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    if (i & 1) {
        var result = test("Cocoa", true);
        shouldBe(result instanceof RegExp, true);
    } else {
        var result = test("Cocoa", false);
        shouldBe(result.input, "Cocoa");
        shouldBe(result[0], "oa");
    }
}
