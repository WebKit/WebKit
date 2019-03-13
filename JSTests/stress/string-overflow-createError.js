//@ skip if $memoryLimited
//@ runDefault
var exception;
try {
    bar = '2.3023e-320'
    foo = bar.padEnd(2147483644, 1);
    foo(true, 1).value;
} catch (e) {
    exception = e;
}

// Creating the error message for the TypeError overflows
// the string and therefore an out-of-memory error is thrown.
if (exception != "Error: Out of memory")
    throw "FAILED";
