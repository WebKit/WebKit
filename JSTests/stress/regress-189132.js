//@ skip if $memoryLimited

try {
    var a0 = '\ud801';
    var a1 = [];
    a2 = a0.padEnd(2147483644,'x');
    a1[a2];
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";

