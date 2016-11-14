//@ if $buildType == "debug" then runFTLNoCJIT("--maxSingleAllocationSize=20000000") else skip end

function shouldEqual(actual, expected) {
    if (actual != expected) {
        throw "ERROR: expect " + expected + ", actual " + actual;
    }
}

s0 = "";
s1 = "NaNxxxxx";

try {
    for (var count = 0; count < 27; count++) {
        var oldS1 = s1;
        s1 += s1;
        s0 = oldS1;
    }
} catch (e) { }

try {
    s1 += s0;
} catch (e) { }

var exception;
try {
    /x/[Symbol.match](s1);
} catch (e) {
    exception = e;
}

shouldEqual(exception, "Error: Out of memory");

