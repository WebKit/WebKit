function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function test1(arg1, arg2, arg3)
    {
        return arguments.length;
    }

    function test()
    {
        shouldBe(test1(), 0);
        shouldBe(test1(0), 1);
        shouldBe(test1(0, 1), 2);
        shouldBe(test1(0, 1, 2), 3);
        shouldBe(test1(0, 1, 2, 3), 4);
    }
    noInline(test);

    for (var i = 0; i < 1e4; ++i)
        test();
}());

(function () {
    function test1(flag, arg1, arg2, arg3)
    {
        if (flag)
            OSRExit();
        return arguments;
    }

    function test(flag)
    {
        shouldBe(test1(flag).length, 1);
        shouldBe(test1(flag, 0).length, 2);
        shouldBe(test1(flag, 0, 1).length, 3);
        shouldBe(test1(flag, 0, 1, 2).length, 4);
        shouldBe(test1(flag, 0, 1, 2, 3).length, 5);
    }
    noInline(test);
    for (var i = 0; i < 1e5; ++i)
        test(false);

    test(true);
    test(true);
    test(true);
}());
