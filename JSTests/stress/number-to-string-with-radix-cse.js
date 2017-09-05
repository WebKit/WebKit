function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var result;
    for (var i = 0; i < 1e2; ++i) {
        i.toString(16);
        i.toString(16);
        i.toString(16);
        i.toString(16);
        result = i.toString(16);
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1e3; ++i)
    shouldBe(test(), `63`);
