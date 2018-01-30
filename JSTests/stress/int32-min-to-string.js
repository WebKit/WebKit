function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test2()
{
    return (-2147483648).toString(2);
}
noInline(test2);

function test4()
{
    return (-2147483648).toString(4);
}
noInline(test4);

function test8()
{
    return (-2147483648).toString(8);
}
noInline(test8);

function test16()
{
    return (-2147483648).toString(16);
}
noInline(test16);

function test32()
{
    return (-2147483648).toString(32);
}
noInline(test32);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(test2(), '-10000000000000000000000000000000');
    shouldBe(test4(), '-2000000000000000');
    shouldBe(test8(), '-20000000000');
    shouldBe(test16(), '-80000000');
    shouldBe(test32(), '-2000000');
}
