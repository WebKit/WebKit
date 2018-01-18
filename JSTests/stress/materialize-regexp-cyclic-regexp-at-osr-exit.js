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
    if (num === 0) {
        OSRExit();
        return regexp;
    }
    return 42;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(1), 42);

var regexp = test(0);
shouldBe(regexp instanceof RegExp, true);
shouldBe(regexp.toString(), "/hello world/");
shouldBe(regexp.lastIndex instanceof RegExp, true);
shouldBe(regexp.lastIndex.toString(), "/World/");
