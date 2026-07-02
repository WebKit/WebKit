function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function inlineTarget(arg1, arg2)
{
    return [arg1, arg2];
}

function test() {
    shouldBe(JSON.stringify(inlineTarget(null)), `[null,null]`);
}
noInline(test);
for (var i = 0; i < 3e4; ++i)
    test();
