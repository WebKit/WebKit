//@ skip if $memoryLimited

var exception;
try {
    Function('a'.repeat(2147483623));
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";

