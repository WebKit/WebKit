//@ runFTLNoCJIT

function shouldEqual(actual, expected) {
    if (actual != expected) {
        throw "ERROR: expect " + expected + ", actual " + actual;
    }
}

var exception;

try {
    Symbol(1)();
} catch (e) {
    exception = e;
}

shouldEqual(exception, "TypeError: Symbol(1) is not a function. (In 'Symbol(1)()', 'Symbol(1)' is a Symbol)");
