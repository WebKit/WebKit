function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: expected:(${expected}),actual:(${actual})`);
}

function toString10(value)
{
    return `${value | 0}`;
}
noInline(toString10);

function test()
{
    shouldBe(toString10(1023), "1023");
    shouldBe(toString10(1024), "1024");
    shouldBe(toString10(1025), "1025");
    shouldBe(toString10(123456), "123456");
    shouldBe(toString10(1000000), "1000000");
    shouldBe(toString10(-1), "-1");
    shouldBe(toString10(-1024), "-1024");
    shouldBe(toString10(-123456), "-123456");
    shouldBe(toString10(2147483647), "2147483647");
    shouldBe(toString10(-2147483648), "-2147483648");
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test();

fullGC();

for (let i = 0; i < testLoopCount; ++i)
    test();
