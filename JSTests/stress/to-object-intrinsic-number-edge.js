var createBuiltin = $vm.createBuiltin;

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
        var object = builtin(42);
        shouldBe(object instanceof Number, true);
        shouldBe(object.valueOf(), 42);

        var object = builtin(42.195);
        shouldBe(object instanceof Number, true);
        shouldBe(object.valueOf(), 42.195);
    }
}());
