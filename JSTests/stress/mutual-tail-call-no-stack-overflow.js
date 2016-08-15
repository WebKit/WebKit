//@ defaultNoSamplingProfilerRun

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

function sloppyCountdown(n) {
    function even(n) {
        if (n == 0)
            return n;
        return odd(n - 1);
    }

    function odd(n) {
        if (n == 1)
            return n;
        return even(n - 1);
    }

    if (n % 2 === 0)
        return even(n);
    else
        return odd(n);
}

function strictCountdown(n) {
    "use strict";

    function even(n) {
        if (n == 0)
            return n;
        return odd(n - 1);
    }

    function odd(n) {
        if (n == 1)
            return n;
        return even(n - 1);
    }

    if (n % 2 === 0)
        return even(n);
    else
        return odd(n);
}

shouldThrow(function () { sloppyCountdown(100000); }, "RangeError: Maximum call stack size exceeded.");
strictCountdown(100000);

// Parity alterning
function odd(n) {
    "use strict";
    if (n > 0)
        return even(n, 0);
}

function even(n) {
    "use strict";
    return odd(n - 1);
}

odd(100000);
