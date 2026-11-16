//@ memoryHog!
(function () {
    function shouldBe(actual, expected) {
        if (actual !== expected)
            throw new Error('bad value: ' + actual);
    }

    function f0(a, b)
    {
        return a + b;
    }

    function f1(a, b)
    {
        return f0(a, b);
    }

    function f2(a, b)
    {
        return f1(a, b);
    }

    function f3(a, b)
    {
        return f2(a, b);
    }
    noInline(f3);

    var string0 = "hello";
    var string1 = "test";
    var string2 = "testing";
    var string3 = "t".repeat(0x7fffffff);
    for (var i = 0; i < testLoopCount; ++i) {
        f3(string0, string1);
        f3(string0, string2);
        f3(string1, string2);
        try {
            f3(string0, string3);
        } catch (e) {
            shouldBe(String(e), `RangeError: Out of memory`);
            shouldBe(e.stack.split('\n').length, 6);
        }
    }
}());
