
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

function sloppy() {
    var object = {};
    Object.defineProperty(object, 'test', {
        writable: false,
        enumerable: true,
        configurable: true,
        value: 42,
    });
    for (const name in object)
        object[name] = 0x1234;
    shouldBe(object.test, 42);
}
noInline(sloppy);

function strict() {
    "use strict";
    var object = {};
    Object.defineProperty(object, 'test', {
        writable: false,
        enumerable: true,
        configurable: true,
        value: 42,
    });
    shouldThrow(() => {
        for (const name in object)
            object[name] = 0x1234;
    }, `TypeError: Attempted to assign to readonly property.`);
}
noInline(strict);

for (var i = 0; i < 1e4; ++i) {
    sloppy();
    strict();
}

