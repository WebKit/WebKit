//@ skip if $architecture != "arm64" and $architecture != "x86_64" and $architecture != "arm" and $architecture != "mips"

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(flag, string)
{
    var regexp = /hello/g;
    regexp.lastIndex = "Cocoa";
    var result = string.match(regexp);
    if (flag)
        return [result, regexp];
    return regexp.lastIndex;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    if (i & 0x1) {
        var [result, regexp] = test(true, "hellohello");
        shouldBe(regexp instanceof RegExp, true);
        shouldBe(regexp.lastIndex, 0);
        shouldBe(result.length, 2);
        shouldBe(result[0], "hello");
        shouldBe(result[1], "hello");
    } else
        shouldBe(test(false, "hellohello"), 0);
}
