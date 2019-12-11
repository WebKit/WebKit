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

(function () {
    "use strict";

    var object = {
        Cocoa: 'Cocoa',
        Cappuccino: 'Cappuccino'
    };

    object.Matcha = 'Matcha';
    shouldBe(object.Matcha, 'Matcha');
    Object.freeze(object);
    shouldBe(object.Matcha, 'Matcha');
    shouldBe(Reflect.isExtensible(object), false);
    shouldThrow(() => object.Mocha = 'Mocha', `TypeError: Attempted to assign to readonly property.`);
}());

(function () {
    "use strict";

    var object = {
        Cocoa: 'Cocoa',
        Cappuccino: 'Cappuccino'
    };

    object.Matcha = 'Matcha';
    shouldBe(object.Matcha, 'Matcha');
    Object.seal(object);
    shouldBe(object.Matcha, 'Matcha');
    shouldBe(Reflect.isExtensible(object), false);
    shouldThrow(() => object.Mocha = 'Mocha', `TypeError: Attempted to assign to readonly property.`);
}());
