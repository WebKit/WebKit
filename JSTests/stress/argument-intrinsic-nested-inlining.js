function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (a) {
    return @argument(0);
})`);

function builtinCaller1()
{
    return builtin();
}

function builtinCaller2()
{
    return builtin(42);
}

function escape(value)
{
    return value;
}
noInline(escape);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(escape(builtinCaller1()), undefined);
        shouldBe(escape(builtinCaller2()), 42);
    }
}());
