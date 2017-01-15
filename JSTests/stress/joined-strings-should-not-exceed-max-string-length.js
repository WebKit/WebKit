//@ skip if $memoryLimited
//@ runFTLNoCJIT if !$memoryLimited
// This test should not crash or fail any assertions.

function shouldEqual(stepId, actual, expected) {
    if (actual != expected) {
        throw stepId + ": ERROR: expect " + expected + ", actual " + actual;
    }
}

var exception = undefined;

arr = [null,null,null,null];
str = "xx";

try {
    for (var i = 0; i < 100; ++i)
        str = arr.join(str);
} catch (e) {
    exception = e;
}
shouldEqual(10000, exception, "Error: Out of memory");

exception = undefined;
try {
    str += 'x';
} catch(e) {
    exception = e;
}
shouldEqual(10100, exception, undefined);
