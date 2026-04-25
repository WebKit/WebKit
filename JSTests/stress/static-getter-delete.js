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

// Before static functions (& accessors) are reified.
shouldThrow(function () {
    'use strict';
    RegExp.prototype.multiline = 'ok';
}, 'TypeError: Attempted to assign to readonly property.');

(function () {
    'use strict';
    shouldBe(delete RegExp.prototype.global, true);
    shouldBe(RegExp.prototype.hasOwnProperty('global'), false);
    RegExp.prototype.global = 'hello'
    shouldBe(RegExp.prototype.global, 'hello');
}());

// After static functions (& accessors) are reified.
shouldThrow(function () {
    'use strict';
    RegExp.prototype.multiline = 'ok';
}, 'TypeError: Attempted to assign to readonly property.');

(function () {
    'use strict';
    shouldBe(delete RegExp.prototype.multiline, true);
    shouldBe(RegExp.prototype.hasOwnProperty('multiline'), false);
    RegExp.prototype.multiline = 'hello'
    shouldBe(RegExp.prototype.multiline, 'hello');
}());
