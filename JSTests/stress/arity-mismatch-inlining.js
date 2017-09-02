function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function inlineTarget(arg1, arg2, arg3, arg4, arg5)
{
    return [arg1, arg2, arg3, arg4, arg5];
}

function test() {
    shouldBe(JSON.stringify(inlineTarget()), `[null,null,null,null,null]`);
    shouldBe(JSON.stringify(inlineTarget(42)), `[42,null,null,null,null]`);
    shouldBe(JSON.stringify(inlineTarget(42, 43)), `[42,43,null,null,null]`);
    shouldBe(JSON.stringify(inlineTarget(42, 43, 44)), `[42,43,44,null,null]`);
    shouldBe(JSON.stringify(inlineTarget(42, 43, 44, 45)), `[42,43,44,45,null]`);
    shouldBe(JSON.stringify(inlineTarget(42, 43, 44, 45, 46)), `[42,43,44,45,46]`);
    shouldBe(JSON.stringify(inlineTarget(42, 43, 44, 45, 46, 47)), `[42,43,44,45,46]`);
    shouldBe(JSON.stringify(inlineTarget(42, 43, 44, 45, 46, 47, 48)), `[42,43,44,45,46]`);
}
noInline(test);
for (var i = 0; i < 3e4; ++i)
    test();
