(function () {
    "use strict";

    function shouldBe(actual, expected)
    {
        if (actual !== expected)
            throw new Error('bad value: ' + actual);
    }
    noInline(shouldBe);

    var flag = false;

    function a(...args)
    {
        return b(...args);
    }

    function b(...args)
    {
        if (flag) {
            OSRExit();
            return args[0];
        }
        return c(...args);
    }

    function c(...args)
    {
        return d(...args);
    }

    function d(...args)
    {
        return Math.max(...args);
    }
    noInline(d);

    var array = [0, 1, 2, 3, 4, 5];
    for (var i = 0; i < 1e6; ++i) {
        flag = i > (1e6 - 100);
        var ret = a(...array);
        if (!flag)
            shouldBe(ret, 5);
        else
            shouldBe(ret, 0);
    }
}());
