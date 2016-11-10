function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (a) {
    a = 42;
    return @argument(0);
})`);
noInline(builtin);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(builtin(), undefined);
        shouldBe(builtin(1), 42);
        shouldBe(builtin(1, 2), 42);
        shouldBe(builtin(1, 2, 3), 42);
    }
}());
