//@ skip if $memoryLimited

var exception;
try {
    new Uint8Array(0x100000000).toString();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED: " + exception;
