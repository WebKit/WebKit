//@ if $memoryLimited then skip else runDefault end

var exception;
try {
    const str = "a".padStart(0x80000000 - 1);
    new Date(str);
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
