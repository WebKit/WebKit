var createBuiltin = $vm.createBuiltin;

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

(function () {
    var builtin = createBuiltin(`(function (target) {
        return @toObject(target, "");
    })`);
    noInline(builtin);

    for (var i = 0; i < 1e4; ++i)
        shouldThrow(() => builtin(null), `TypeError: null is not an object`);
}());

(function () {
    var builtin = createBuiltin(`(function (target) {
        return @toObject(target, "");
    })`);
    noInline(builtin);

    for (var i = 0; i < 1e4; ++i)
        shouldThrow(() => builtin(undefined), `TypeError: undefined is not an object`);
}());
