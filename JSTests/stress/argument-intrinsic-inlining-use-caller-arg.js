function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    return 42;
}
noInline(test);

var builtin = createBuiltin(`(function (a) {
    return @argument(2);
})`);

function inlining(a, b, c)
{
    return builtin(1, 2, test(), 4, 5, 6, 7);
}
noInline(inlining);

function escape(value)
{
    return value;
}
noInline(escape);

(function () {
    for (var i = 0; i < 1e4; ++i)
        shouldBe(escape(inlining(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)), 42);
}());
