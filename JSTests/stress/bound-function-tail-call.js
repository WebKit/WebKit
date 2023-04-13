//@ requireOptions("--useConcurrentJIT=0", "--dfgAllowlist=test4", "--useFTLJIT=0")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function entry() {
    "use strict";
    var count = 0;
    var flag = false;
    flag = true;
    flag = false;
    function test() {
        ++count;
        return 42;
    }
    noInline(test);

    function test2()
    {
        ++count;
        return test();
    }

    var call = test2.call.bind(test2);

    function test3() {
        ++count;
        var a, b, c, d, e, f, g, h, i, j;
        return call(0, 1, 2);
    }

    function test4() {
        ++count;
        return test3();
    }
    noInline(test4);

    for (var i = 0; i < 3000; ++i) {
        count = 0;
        shouldBe(test4(), 42);
    }
}());
