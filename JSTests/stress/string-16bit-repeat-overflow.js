//@ skip if $memoryLimited
var exception;
try {
    print('\ud000'.repeat(2**31));
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
