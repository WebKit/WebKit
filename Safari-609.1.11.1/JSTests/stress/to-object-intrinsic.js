var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var builtin = createBuiltin(`(function (target) {
    return @toObject(target, "");
})`);
noInline(builtin);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        shouldThrow(() => builtin(null), `TypeError: null is not an object`);
        shouldThrow(() => builtin(undefined), `TypeError: undefined is not an object`);
        var object = builtin(42);
        shouldBe(object instanceof Number, true);
        shouldBe(object.valueOf(), 42);

        var object = builtin("Cocoa");
        shouldBe(object instanceof String, true);
        shouldBe(object.valueOf(), "Cocoa");

        var object = builtin(true);
        shouldBe(object instanceof Boolean, true);
        shouldBe(object.valueOf(), true);

        var object = builtin(Symbol("Cocoa"));
        shouldBe(object instanceof Symbol, true);
        shouldBe(String(object.valueOf()), `Symbol(Cocoa)`);

        var arg = {};
        var object = builtin(arg);
        shouldBe(object, arg);
    }
}());

