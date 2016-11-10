function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (a) {
    return @argument(0);
})`);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(builtin(), undefined);
        shouldBe(builtin(42), 42);
    }
}());
