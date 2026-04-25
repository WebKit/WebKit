//@ runDefault

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

"use strict";

function bar(x, y) {
    function foo(a, b) {
        if (a == 0) b += ',';
        return foo(b - 1, a, 43);
    }
    return foo(x, y);
}

shouldThrow(() => {
    bar(1, 1);
}, "RangeError: Maximum call stack size exceeded.");
