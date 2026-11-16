//@ runFTLNoCJIT
// This test passes if it does not crash.

function shouldEqual(testId, actual, expected) {
    if (actual != expected) {
        throw testId + ": ERROR: expect " + expected + ", actual " + actual;
    }
}

arr = new Array;

Object.defineProperty(arr, 1, {
    configurable: true, enumerable: true,
    get: Date.prototype.getSeconds,
});

typedArray = new Float64Array(16);
typedArray[0] = 0;

var exception = undefined;
try {
    typedArray.set(arr, 0);
} catch (e) {
    exception = e;
}

shouldEqual(10000, exception, "TypeError: Type error");
