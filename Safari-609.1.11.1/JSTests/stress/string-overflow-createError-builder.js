//@ skip if $memoryLimited
//@ runDefault
var exception;
try {
    bar = '2.3023e-320'
    foo = bar.padEnd(2147483620, 1);
    foo(true, 1).value;
} catch (e) {
    exception = e;
}

// when the StringBuilder for the error message overflows,
// "object is not a function" is used as message for the TypeError.
if (exception.message != "object is not a function."
    && exception != "Error: Out of memory")
    throw "FAILED";
