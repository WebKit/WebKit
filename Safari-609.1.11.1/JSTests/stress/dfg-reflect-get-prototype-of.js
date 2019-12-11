function shouldBe(actual, expected)
{
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

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target({}), Object.prototype);
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i) {
        shouldThrow(() => target(null), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(undefined), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
    }
}());

(function () {
    function target(object)
    {
        return Reflect.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i) {
        shouldThrow(() => target("Cocoa"), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(42), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
        shouldThrow(() => target(true), `TypeError: Reflect.getPrototypeOf requires the first argument be an object`);
    }
}());
