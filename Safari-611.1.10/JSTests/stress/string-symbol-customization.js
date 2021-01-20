function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
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

shouldThrow(() => {
    "Cocoa".search({
        [Symbol.search]: 42
    });
}, `TypeError: 42 is not a function`);

shouldThrow(() => {
    "Cocoa".match({
        [Symbol.match]: 42
    });
}, `TypeError: 42 is not a function`);

shouldThrow(() => {
    "Cocoa".search({
        [Symbol.search]: {}
    });
}, `TypeError: Object is not a function`);

shouldThrow(() => {
    "Cocoa".match({
        [Symbol.match]: {}
    });
}, `TypeError: Object is not a function`);

shouldBe("Cocoa".search({
    [Symbol.search]: null,
    toString()
    {
        return "C"
    }
}), 0);

shouldBe("Cocoa".match({
    [Symbol.match]: null,
    toString()
    {
        return "C"
    }
})[0], "C");

shouldBe("Cocoa".search({
    [Symbol.search]: undefined,
    toString()
    {
        return "C"
    }
}), 0);

shouldBe("Cocoa".match({
    [Symbol.match]: undefined,
    toString()
    {
        return "C"
    }
})[0], "C");

shouldBe("Cocoa".search({
    [Symbol.search]()
    {
        return 42;
    }
}), 42);

shouldBe("Cocoa".match({
    [Symbol.match]()
    {
        return 42;
    }
}), 42);

RegExp.prototype[Symbol.search] = function () { return 42; };
RegExp.prototype[Symbol.match] = function () { return 42; };

shouldBe("Cocoa".search({
    [Symbol.search]: null
}), 42);

shouldBe("Cocoa".match({
    [Symbol.match]: null
}), 42);

shouldBe("Cocoa".search({
    [Symbol.search]: undefined
}), 42);

shouldBe("Cocoa".match({
    [Symbol.match]: undefined
}), 42);
