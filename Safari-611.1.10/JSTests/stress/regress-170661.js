function f() {
    (function bar() {
        eval('1');
        f();
    }());

    throw 1;
}

var exception;
try {
    f();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw("FAILED");
