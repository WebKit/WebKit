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
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target({}), Object.prototype);
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i) {
        shouldThrow(() => target(null), `TypeError: null is not an object (evaluating 'Object.getPrototypeOf(object)')`);
        shouldThrow(() => target(undefined), `TypeError: undefined is not an object (evaluating 'Object.getPrototypeOf(object)')`);
    }
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target("Cocoa"), String.prototype);
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target(42), Number.prototype);
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target(42.195), Number.prototype);
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target(true), Boolean.prototype);
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i)
        shouldBe(target(Symbol("Cocoa")), Symbol.prototype);
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(target("Cocoa"), String.prototype);
        shouldBe(target(42), Number.prototype);
        shouldBe(target(42.195), Number.prototype);
        shouldBe(target(true), Boolean.prototype);
        shouldBe(target(Symbol("Cocoa")), Symbol.prototype);
    }
}());
