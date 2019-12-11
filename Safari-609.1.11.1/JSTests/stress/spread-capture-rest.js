(function () {
    "use strict";

    function shouldBe(actual, expected)
    {
        if (actual !== expected)
            throw new Error('bad value: ' + actual);
    }
    noInline(shouldBe);

    function capture(arg)
    {
    }
    noInline(capture);

    var flag = false;

    function a(...args)
    {
        // This makes args and Spread non-phantom.
        capture(args);
        if (flag) {
            OSRExit();
            return args[0];
        }
        return b(...args);
    }

    function b(...args)
    {
        return Math.max(...args);
    }

    for (var i = 0; i < 1e6; ++i) {
        flag = i > (1e6 - 100);
        var ret = a(0, 1, 2, 3, 4);
        if (!flag)
            shouldBe(ret, 4);
        else
            shouldBe(ret, 0);
    }
}());

(function () {
    "use strict";

    function shouldBe(actual, expected)
    {
        if (actual !== expected)
            throw new Error('bad value: ' + actual);
    }
    noInline(shouldBe);

    function capture(arg)
    {
    }
    noInline(capture);

    var flag = false;

    function a(...args)
    {
        // This makes args and Spread non-phantom.
        capture(args);
        if (flag) {
            OSRExit();
            return args[0];
        }
        return Math.max(...args);
    }

    for (var i = 0; i < 1e6; ++i) {
        flag = i > (1e6 - 100);
        var ret = a(0, 1, 2, 3, 4);
        if (!flag)
            shouldBe(ret, 4);
        else
            shouldBe(ret, 0);
    }
}());
