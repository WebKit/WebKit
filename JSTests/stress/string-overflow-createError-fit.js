//@ skip if $memoryLimited
//@ runDefault
var exception;
try {
    bar = '2.3023e-320'
    foo = bar.padEnd(2147480000, 1);
    foo(true, 1).value;
} catch (e) {
    exception = e;
}

// Although the message of the TypeError is quite long,
// it still fits into String::MaxLength. Check the start
// of the error message.
if (!exception.message.startsWith("foo is not a function")
    && exception != "Error: Out of memory")
    throw "FAILED";
