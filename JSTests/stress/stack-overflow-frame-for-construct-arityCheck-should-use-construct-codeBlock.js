//@ requireOptions("--maxPerThreadStackUsage=1572864")

function foo(a, b, c) {
    try {
        throw new Error();
    } catch {
        hello();
    }
};

function Bar(d, e) {
    hello();
}

function hello(f) {
    new Bar(0);
};

var exception;
try {
    foo();
} catch(e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";
