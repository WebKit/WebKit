//@ if $buildType == "release" && !$memoryLimited then runDefault else skip end

var exception;
try {
    unescape('\u0100'.repeat(2**30));
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
