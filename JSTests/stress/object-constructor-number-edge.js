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
        var object = test(42);
        shouldBe(object instanceof Number, true);
        shouldBe(object.valueOf(), 42);

        var object = test(42.195);
        shouldBe(object instanceof Number, true);
        shouldBe(object.valueOf(), 42.195);
    }
}());
