function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1()
{
    return Math.min(0, -1, -2, -3, -4);
}
noInline(test1);

function test2()
{
    return Math.max(0, -1, -2, -3, -4, 20000);
}
noInline(test2);

function test3()
{
    return Math.min(0.1, 0.2, 0.3, -1.3);
}
noInline(test3);

function test4()
{
    return Math.max(0.1, 0.2, 0.3, -1.3, 2.5);
}
noInline(test4);

function test5()
{
    return Math.min(0.1, 0.2, 0.3, -1.3, NaN);
}
noInline(test5);

function test6()
{
    return Math.max(0.1, 0.2, 0.3, -1.3, 2.5, NaN);
}
noInline(test6);

for (let i = 0; i < 1e5; ++i) {
    shouldBe(test1(), -4);
    shouldBe(test2(), 20000);
    shouldBe(test3(), -1.3);
    shouldBe(test4(), 2.5);
    shouldBe(Number.isNaN(test5()), true);
    shouldBe(Number.isNaN(test6()), true);
}
