//@ skip if $memoryLimited
//@ runDefault

try {
    ''.padStart(2**31-1)();
} catch(e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
