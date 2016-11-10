function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (a) {
    return @argument(1);
})`);
noInline(builtin);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(builtin(), undefined);
        shouldBe(builtin(1), undefined);
        shouldBe(builtin(1, 2), 2);
        shouldBe(builtin(1, 2, 3), 2);
    }
}());
