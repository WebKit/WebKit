function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function test2(...rest)
    {
        return rest;
    }

    function test1(arg1, arg2, arg3)
    {
        return test2(arg1, arg2, arg3);
    }

    function test()
    {
        var result = test1();
        shouldBe(result.length, 3);
        shouldBe(result[0], undefined);
        shouldBe(result[1], undefined);
        shouldBe(result[2], undefined);
    }
    noInline(test);

    for (var i = 0; i < 1e4; ++i)
        test();
}());

(function () {
    function test1(...rest)
    {
        return rest;
    }

    function test()
    {
        var result = test1();
        shouldBe(result.length, 0);
    }
    noInline(test);

    for (var i = 0; i < 1e4; ++i)
        test();
}());

(function () {
    function test1(...rest)
    {
        return rest;
    }
    noInline(test1);

    function test()
    {
        var result = test1();
        shouldBe(result.length, 0);
    }
    noInline(test);

    for (var i = 0; i < 1e4; ++i)
        test();
}());
