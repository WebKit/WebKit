//@ requireOptions("--maxPerThreadStackUsage=300000", "--exceptionStackTraceLimit=0", "--defaultErrorStackTraceLimit=0")

function bar(v) {
    !v
    foo();
}
function foo() {
    eval(`bar(import(0));`);
}

var exception;
try {
    foo();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";
