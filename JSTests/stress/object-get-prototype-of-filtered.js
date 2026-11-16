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

// In this case, we cannot handle it as GetPrototypeOf since GetById is opaque.

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e4; ++i) {
        var object = {};
        object[`Cocoa${i}`] = `Cocoa`;
        shouldBe(target(object), Object.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e4; ++i) {
        var array = [];
        array[`Cocoa${i}`] = `Cocoa`;
        shouldBe(target(array), Array.prototype);
    }
}());

(function () {
    function target(object)
    {
        return Object.getPrototypeOf(object);
    }
    noInline(target);

    for (var i = 0; i < 1e4; ++i) {
        function Cocoa() { }
        Cocoa[`Cocoa${i}`] = `Cocoa`;
        shouldBe(target(Cocoa), Function.prototype);
    }
}());
