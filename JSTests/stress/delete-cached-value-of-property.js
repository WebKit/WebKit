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

var object = {
    __proto__: {
        toString: undefined,
        valueOf: function () {
            return 42;
        }
    }
};

shouldBe(object + 42, 84);
shouldBe(object + 42, 84);
delete object.__proto__.valueOf;
shouldThrow(() => {
    object + 42;
}, `TypeError: No default value`);
