function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test2()
{
    return arguments.length;
}

function test(input)
{
    return test2.apply(null, input);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(null), 0);
