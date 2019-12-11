function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    for (var i = 0; i < 10; ++i) {
        var result = '';
        result += i.toString(2);
        result += i.toString(4);
        result += i.toString(8);
        result += i.toString(16);
        result += i.toString(32);
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldBe(test(), `1001211199`);
