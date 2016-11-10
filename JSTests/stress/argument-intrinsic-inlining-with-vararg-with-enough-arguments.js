function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (a) {
    return @argument(5);
})`);

function inlining(a, b, c)
{
    return builtin.call(this, ...[1, 2, 3, 4, 5, 6, 7]);
}
noInline(inlining);

function escape(value)
{
    return value;
}
noInline(escape);

(function () {
    for (var i = 0; i < 1e4; ++i)
        shouldBe(escape(inlining(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)), 6);
}());
