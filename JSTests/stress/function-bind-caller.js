function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function inlined(a, b, c)
{
    shouldBe(inlined.caller, test);
}

var bound = inlined.bind(undefined, 3).bind(undefined, 1, 2);

function test()
{
    return bound();
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
