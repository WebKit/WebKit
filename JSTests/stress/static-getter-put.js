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

shouldThrow(function () {
    'use strict';
    RegExp.prototype.global = 'Cocoa';
}, 'TypeError: Attempted to assign to readonly property.');

// Twice.
shouldThrow(function () {
    'use strict';
    RegExp.prototype.global = 'Cocoa';
}, 'TypeError: Attempted to assign to readonly property.');

(function () {
    'use strict';
    delete RegExp.prototype.global;
    RegExp.prototype.global = 'Cocoa';
    shouldBe(RegExp.prototype.global, 'Cocoa');
    shouldBe(/Cappuccino/.global, 'Cocoa');
}());
