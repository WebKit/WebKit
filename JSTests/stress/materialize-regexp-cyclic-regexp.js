//@ skip if $architecture != "arm64" and $architecture != "x86_64" and $architecture != "arm" and $architecture != "mips"

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(num)
{
    var regexp = /hello world/;
    var world = /World/;
    regexp.lastIndex = world;
    world.lastIndex = regexp;
    if (num === 0)
        return regexp;
    if (num === 1)
        return regexp.lastIndex;
    return regexp.lastIndex.lastIndex;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var num = i % 3;
    switch (num) {
    case 0:
        var regexp = test(num);
        shouldBe(regexp instanceof RegExp, true);
        shouldBe(regexp.toString(), "/hello world/");
        shouldBe(regexp.lastIndex instanceof RegExp, true);
        shouldBe(regexp.lastIndex.toString(), "/World/");
        break;
    case 1:
        var regexp = test(num);
        shouldBe(regexp instanceof RegExp, true);
        shouldBe(regexp.toString(), "/World/");
        shouldBe(regexp.lastIndex instanceof RegExp, true);
        shouldBe(regexp.lastIndex.toString(), "/hello world/");
        break;
    case 2:
        var regexp = test(num);
        shouldBe(regexp instanceof RegExp, true);
        shouldBe(regexp.toString(), "/hello world/");
        shouldBe(regexp.lastIndex instanceof RegExp, true);
        shouldBe(regexp.lastIndex.toString(), "/World/");
        break;
    }
}
