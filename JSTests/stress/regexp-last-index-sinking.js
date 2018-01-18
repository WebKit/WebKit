function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(num)
{
    var regexp = /regexp/;
    if (num & 1)
        regexp.lastIndex = 42;
    else
        regexp.lastIndex = 2;
    return regexp.lastIndex;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    if (i & 1)
        shouldBe(test(i), 42);
    else
        shouldBe(test(i), 2);
}
