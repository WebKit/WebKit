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

function sloppyLoop(n) {
    if (n > 0)
        return sloppyLoop(n - 1);
}

function strictLoop(n) {
    "use strict";
    if (n > 0)
        return strictLoop(n - 1);
}

// We have two of these so that we can test different stack alignments
function strictLoopArityFixup1(n, dummy) {
    "use strict";
    if (n > 0)
        return strictLoopArityFixup1(n - 1);
}

function strictLoopArityFixup2(n, dummy1, dummy2) {
    "use strict";
    if (n > 0)
        return strictLoopArityFixup2(n - 1);
}

shouldThrow(function () { sloppyLoop(100000); }, 'RangeError: Maximum call stack size exceeded.');

// These should not throw
strictLoop(100000);
strictLoopArityFixup1(1000000);
strictLoopArityFixup2(1000000);
