function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(target)
{
    return Object(target);
}
noInline(test);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        var arg = {};
        var object = test(arg);
        shouldBe(object, arg);

        var arg = [];
        var object = test(arg);
        shouldBe(object, arg);

        var arg = function () { };
        var object = test(arg);
        shouldBe(object, arg);
    }
}());
