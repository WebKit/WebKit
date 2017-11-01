function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function (target) {
    return @toObject(target, "");
})`);
noInline(builtin);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        var object = builtin(true);
        shouldBe(object instanceof Boolean, true);
        shouldBe(object.valueOf(), true);
    }
}());
