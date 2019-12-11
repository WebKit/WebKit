function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(flag)
{
    var regexp = /hello world/;
    regexp.lastIndex = "Cocoa";
    if (flag) {
        OSRExit();
        return regexp;
    }
    return regexp.lastIndex;
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test(false), "Cocoa");

var regexp = test(true);
shouldBe(regexp instanceof RegExp, true);
shouldBe(regexp.lastIndex, "Cocoa");
