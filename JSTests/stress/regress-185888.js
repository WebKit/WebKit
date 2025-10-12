//@ if $memoryLimited then skip else runDefault end

var exception;
try {
    const str = "a".padStart(0x80000000);
    new Date(str);
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
