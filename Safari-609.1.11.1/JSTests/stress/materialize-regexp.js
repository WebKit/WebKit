function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(flag)
{
    var regexp = /hello world/;
    regexp.lastIndex = "Cocoa";
    if (flag)
        return regexp;
    return regexp.lastIndex;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    if (i & 0x1) {
        var regexp = test(true);
        shouldBe(regexp instanceof RegExp, true);
        shouldBe(regexp.lastIndex, "Cocoa");
    } else
        shouldBe(test(false), "Cocoa");
}
