function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (a) {
    return @argument(0);
})`);

function escape(value)
{
    return value;
}
noInline(escape);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(escape(builtin()), undefined);
        shouldBe(escape(builtin(42)), 42);
    }
}());
